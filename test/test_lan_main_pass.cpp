#include "extension_lan_state.hpp"
#include "grass_lan_state.hpp"
#include "transport_contract.hpp"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <vector>

namespace ext = aquarium::extension_lan;
namespace grass = aquarium::grass_lan;
std::uint64_t clock_ms = 1000;
std::uint64_t monotonic_millis() { return clock_ms; }
constexpr std::time_t kMinimumReasonableEpochSeconds = 1700000000;
struct { bool bark_enabled = true; } runtime_config;
ext::ConnectivityTracker extension_connectivity_tracker;
grass::ConnectivityTracker grass_connectivity_tracker;
grass::State grass_state;
enum class DiagnosticEvent { extension_stale, extension_recovered, grass_stale, grass_recovered };
void record_diagnostic(DiagnosticEvent, std::uint64_t, std::uint32_t, std::uint32_t) {}
void record_extension_packet(const ext::State&) {}
void record_grass_packet(const grass::State&) {}
void connect_wifi(std::uint64_t) {}
void observe_time_sync() {}

void queue_grass(unsigned sequence) {
  grass::State s{};
  s.sampled_at_ms = sequence * 30000U;
  s.temperature_c[0] = 25.0F;
  s.thermal_state[0] = grass::ThermalState::normal;
  const auto p = grass::encode(s, 9, sequence, {});
  WiFiUDP::queues[35115].push_back({p.begin(), p.end()});
}
struct {
  std::vector<aquarium::transport::BarkMessage> messages;
  bool notify(const aquarium::transport::BarkMessage& message) {
    messages.push_back(message);
    // Only the external HTTP wait is substituted. Receiver, HMAC, state
    // transitions, message creation and main-loop ordering are production code.
    if (messages.size() == 1) {
      clock_ms = 91000;
      queue_grass(4);
    }
    return true;
  }
} bark;

void connectivity_pass() {
  std::uint64_t now_ms = monotonic_millis();
  // Generated verbatim from main.cpp's connectivity portion by the runner.
#include "lan_main_pass.inc"
}

int main() {
  ext::begin(); grass::begin();
  ext::State s{};
  s.temperature_c[1] = 26.0F;
  s.thermal_state[1] = ext::ThermalState::normal;
  const auto p = ext::encode(s, 7, 1, {});
  WiFiUDP::queues[35112].push_back({p.begin(), p.end()});
  queue_grass(1);
  connectivity_pass();
  assert(bark.messages.empty());
  clock_ms = 76001;
  connectivity_pass();
  assert(bark.messages.size() == 1U && "new ESP3 packet during ESP2 Bark must prevent stale-snapshot alarm");
  assert(grass_state.fresh);
  clock_ms = 166001;
  connectivity_pass();
  assert(bark.messages.size() == 2U);
  connectivity_pass();
  assert(bark.messages.size() == 2U);
  queue_grass(7); clock_ms = 181000;
  connectivity_pass();
  assert(bark.messages.size() == 3U);
  connectivity_pass();
  assert(bark.messages.size() == 3U);
  std::puts("PASS main_loop_packet_during_bark_and_real_outage");
}
