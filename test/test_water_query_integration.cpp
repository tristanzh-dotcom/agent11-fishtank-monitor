#include "tab5_lan_state.hpp"
#include "grass_lan_state.hpp"
#include "extension_lan_state.hpp"
#include "extension_protocol.hpp"
#include "legacy_lan_state.hpp"
#include "water_target.hpp"
#include "water_preparation.hpp"
#include "daily_summary.hpp"
#include "heartbeat_contract.hpp"
#include "tab5/agent11_lan_state.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace ext = aquarium::extension;
namespace rx = aquarium::extension_lan;

namespace {
constexpr std::array<std::uint8_t, 32> kKey{};
std::uint32_t test_now_ms{};
std::uint32_t millis() { return test_now_ms; }
ext::LegacyLanReducer legacy_reducer{};
ext::Snapshot local_snapshot{};
constexpr std::uint64_t kTemperatureFreshnessMs = 75000U;
#include "esp2_current_target.inc"

void actual_target_route_keeps_the_valid_return_probe() {
  aquarium::ActiveEventSnapshot active;
  active.apply({aquarium::TemperatureEvent{aquarium::EventType::sensor_fault,
      aquarium::EventState::opened, aquarium::Severity::n2, 1000U, 0.0}});
  const auto lan = aquarium::tab5::make_lan_state(
      {1000U, std::nullopt, 28.0}, {25.0, 24.0, 23.0}, {}, active, 28.0, 5000U);
  assert(lan.sensor_fault);
  const auto packet = aquarium::tab5::encode_packet(lan, 7U, 1U, kKey);
  assert(ext::acceptLegacyLanPacket(&legacy_reducer, packet, 1000U, kKey));
  local_snapshot.sampled_at_ms = 1000U;
  local_snapshot.temperature_c[1] = 29.0F;
  test_now_ms = 1000U;
  const auto target = current_target();
  assert(target.sample_count == 6U);
  assert(std::fabs(*target.target_c - 26.4666666667) < 0.0001);
  test_now_ms = 6001U;
  assert(current_target().sample_count == 5U);
  test_now_ms = 76001U;
  assert(!current_target().target_c.has_value());
}


// Real ESP1 encoder -> ESP2 decoder -> six-tank target -> unchanged Tab5 decoder.
void grass_relay_retains_source_expiry_and_target_weight() {
  aquarium::ActiveEventSnapshot active;
  const auto lan = aquarium::tab5::make_lan_state(
      {1000U, 26.0, 28.0}, {25.0, 24.0, 23.0}, {}, active, 28.0, 5000U);
  const auto packet = aquarium::tab5::encode_packet(lan, 7U, 1U, kKey);
  ext::LegacyLanReducer reducer{};
  assert(ext::acceptLegacyLanPacket(&reducer, packet, 1000U, kKey));
  const auto state = ext::freshLegacyLanState(reducer, 1000U);
  ext::FishTankSamples samples{};
  samples.package_main_c = state.package_main_c;
  samples.package_return_c = state.package_return_c;
  samples.laosi_c = state.auxiliary_c[0];
  samples.xiaohei_c = state.auxiliary_c[1];
  samples.maomao_c = state.auxiliary_c[2];
  samples.grass_c = state.grass_c;
  samples.pleco_c = 29.0;
  const auto target = ext::averageValidFishTankSamples(samples);
  assert(target.sample_count == 6U);
  assert(std::fabs(*target.target_c - 26.3) < 0.0001);
  ext::WaterPreparationController controller;
  controller.observe({1000U, ext::SwitchState::off, 24.0, target.target_c});
  controller.observe({2000U, ext::SwitchState::on, 24.0, 29.3});
  assert(std::fabs(*controller.target_c() - 26.3) < 0.0001);
  assert(!ext::freshLegacyLanState(reducer, 6001U).grass_c.has_value());
  auto fresh_primary = ext::freshLegacyLanState(reducer, 6001U);
  samples.grass_c = fresh_primary.grass_c;
  assert(ext::averageValidFishTankSamples(samples).sample_count == 5U);

  tab5::Agent11LanReducer tab5_reducer{};
  assert(tab5::acceptAgent11LanPacket(&tab5_reducer, packet, 1000U, kKey));
  assert(tab5::reduceAgent11LanFreshness(tab5_reducer, 1000U).display_temperature_c == 26.0F);

  // Legacy packets have zero extension bytes, not a real 0 C grass sample.
  auto old = packet;
  old[27] = old[37] = old[38] = old[39] = 0U;
  old[19] = 2U;
  assert(tab5::signAgent11LanPacket(&old, kKey));
  assert(ext::acceptLegacyLanPacket(&reducer, old, 7000U, kKey));
  assert(!ext::freshLegacyLanState(reducer, 7000U).grass_c.has_value());
}

// Real ESP2 encoder -> ESP1 decoder -> query suffix and unchanged Tab5 display.
void water_display_crosses_only_the_query_path() {
  ext::Snapshot source{};
  source.sampled_at_ms = 1000U;
  source.temperature_c[1] = 27.1F;
  source.thermal_state[1] = ext::ThermalState::normal;
  source.water_temperature_c = 26.3F;
  source.water_state = ext::WaterDisplayState::active;
  const auto packet = ext::encode(source, 9U, 1U, kKey);
  rx::Reducer reducer{};
  assert(rx::accept(&reducer, packet, 1000U, kKey));
  const auto state = rx::freshState(reducer, 1000U);
  assert(state.water_state == rx::WaterDisplayState::active);
  assert(*state.water_temperature_c == 26.3F);

  tab5::Agent11ExtensionLanReducer tab5_reducer{};
  assert(tab5::acceptAgent11ExtensionLanPacket(&tab5_reducer, packet, 1000U, kKey));
  assert(tab5::reduceAgent11ExtensionLanFreshness(tab5_reducer, 1000U)
             .extension_temperature_c[1] == 27.1F);

  aquarium::transport::DailyTemperatureSummary summary{};
  summary.snapshot.sampled_at = {2026, 10, 2, 12, 0, 0, true};
  const aquarium::transport::WaterQueryReading reading{
      static_cast<double>(*state.water_temperature_c), state.fresh,
      static_cast<aquarium::transport::WaterQueryState>(state.water_state)};
  const auto original = aquarium::transport::daily_summary_body(summary);
  assert(aquarium::transport::temperature_query_body(summary, reading) ==
         original + "\n养水缸：26.3°C（养水中）");
  assert(aquarium::transport::daily_summary_message(summary, "test").body.find("养水缸") == std::string::npos);

  // No display extension from older ESP2 -> unavailable temperature, unknown state.
  auto old = packet;
  old[34] = old[35] = old[36] = old[37] = 0U;
  old[19] = 2U;
  assert(tab5::signAgent11ExtensionLanPacket(&old, kKey));
  assert(rx::accept(&reducer, old, 2000U, kKey));
  const auto legacy = rx::freshState(reducer, 2000U);
  assert(!legacy.water_temperature_c.has_value());
  assert(legacy.water_state == rx::WaterDisplayState::unknown);

  auto tampered = packet;
  tampered[35] ^= 1U;
  assert(!rx::accept(&reducer, tampered, 3000U, kKey));
  auto malformed = packet;
  malformed[19] = 3U;
  malformed[37] = 3U;
  assert(tab5::signAgent11ExtensionLanPacket(&malformed, kKey));
  assert(!rx::accept(&reducer, malformed, 3000U, kKey));
}

void longest_query_stays_inside_existing_cloud_limit() {
  using namespace aquarium::transport;
  DailyTemperatureSummary summary{};
  auto& s = summary.snapshot;
  s.sampled_at = {2026, 10, 2, 12, 0, 0, true};
  s.main_c = s.sump_c = 100.0;
  s.main_status = TemperatureReadingStatus::high_critical;
  for (std::size_t i = 0U; i < 3U; ++i) {
    s.auxiliary_c[i] = 100.0;
    s.auxiliary_states[i] = SummaryReadingState::valid;
    s.auxiliary_status[i] = TemperatureReadingStatus::high_critical;
  }
  for (auto& tank : s.extension_tanks) {
    tank.temperature_c = 100.0;
    tank.state = SummaryReadingState::valid;
    tank.status = TemperatureReadingStatus::high_critical;
  }
  const auto query = temperature_query_body(summary, {100.0, false, WaterQueryState::unknown});
  assert(query.size() <= aquarium::heartbeat::kMaxTemperatureSummaryBytes);
}
}  // namespace

int main() {
  actual_target_route_keeps_the_valid_return_probe();
  grass_relay_retains_source_expiry_and_target_weight();
  water_display_crosses_only_the_query_path();
  longest_query_stays_inside_existing_cloud_limit();
  std::cout << "water query integration and Tab5 compatibility tests passed\n";
}
