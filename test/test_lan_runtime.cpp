#include "extension_lan_state.hpp"
#include "grass_lan_state.hpp"
#include <WiFiUdp.h>
#include <WiFi.h>
#include <cassert>
#include <cstdio>
#include <string>

namespace ext = aquarium::extension_lan;
namespace grass = aquarium::grass_lan;
const std::array<std::uint8_t, 32> key{};

template <class Packet> void queue(unsigned port, const Packet& packet) {
  WiFiUDP::queues[port].push_back({packet.begin(), packet.end()});
}
auto extPacket(unsigned seq) {
  ext::State s{};
  s.sampled_at_ms = seq * 10000U;
  s.temperature_c[1] = 26.0F;
  s.thermal_state[1] = ext::ThermalState::normal;
  return ext::encode(s, 7U, seq, key);
}
auto grassPacket(unsigned seq, unsigned source = 9U) {
  grass::State s{};
  s.sampled_at_ms = seq * 30000U;
  s.temperature_c[0] = 25.25F;
  s.thermal_state[0] = grass::ThermalState::normal;
  return grass::encode(s, source, seq, key);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  ext::begin(); grass::begin();
  const std::string scenario = argv[1];
  if (scenario == "malformed") {
    queue(35112, std::array<std::uint8_t, 3>{1, 2, 3});
    queue(35112, extPacket(1));
    for (unsigned i = 0; i < 10; ++i) ext::tick(1000U + i);
    assert(ext::snapshot(1010U).fresh && "bad length must not wedge subsequent reception");
    assert(ext::snapshot(1010U).temperature_c[1] == 26.0F);
    assert(ext::receiveCounters().received == 2U);
    assert(ext::receiveCounters().accepted == 1U);
    assert(ext::receiveCounters().rejected == 1U);
  } else if (scenario == "bounded") {
    for (unsigned i = 1; i <= 6; ++i) queue(35112, extPacket(i));
    queue(35115, grassPacket(1));
    ext::tick(1000); grass::tick(1000);
    assert(ext::snapshot(1000).sequence == 4U && "receive a bounded burst of four packets");
    assert(grass::snapshot(1000).temperature_c[0] == 25.25F);
    ext::tick(1050);
    assert(ext::snapshot(1050).sequence == 6U);
  } else if (scenario == "connectivity") {
    ext::ConnectivityTracker et{};
    grass::ConnectivityTracker gt{};
    queue(35112, extPacket(1)); queue(35115, grassPacket(1));
    ext::tick(1000); grass::tick(1000);
    assert(ext::observeConnectivity(&et, ext::snapshot(1000)) == ext::ConnectivityEvent::none);
    assert(grass::observeConnectivity(&gt, grass::snapshot(1000)) == grass::ConnectivityEvent::none);
    assert(grass::snapshot(76000).fresh);
    // Duplicate and bad signature traffic must not extend freshness.
    queue(35115, grassPacket(1));
    auto bad = grassPacket(2); bad[40] ^= 1;
    queue(35115, bad); grass::tick(76001);
    assert(grass::observeConnectivity(&gt, grass::snapshot(76001)) == grass::ConnectivityEvent::offline);
    assert(grass::observeConnectivity(&gt, grass::snapshot(76002)) == grass::ConnectivityEvent::none);
    queue(35115, grassPacket(4)); grass::tick(91000);
    assert(grass::observeConnectivity(&gt, grass::snapshot(91000)) == grass::ConnectivityEvent::recovered);
    assert(grass::observeConnectivity(&gt, grass::snapshot(91001)) == grass::ConnectivityEvent::none);
    assert(!ext::snapshot(91000).fresh);
    assert(grass::receiveCounters().received == 4U);
    assert(grass::receiveCounters().accepted == 2U);
    assert(grass::receiveCounters().rejected == 2U);
    const auto recovered = grass::snapshot(91000);
    assert(recovered.received_at_ms == 91000U);
    assert(recovered.receive_gap_ms == 90000U);
    assert(recovered.missed_frames == 2U);
    assert(recovered.sequence == 4U);
    queue(35115, grassPacket(1, 10)); grass::tick(92000);
    const auto restarted = grass::snapshot(92000);
    assert(restarted.sender_session_changed);
    assert(restarted.receive_gap_ms == 0U && restarted.missed_frames == 0U);
    queue(35115, grassPacket(5)); grass::tick(93000);
    assert(grass::snapshot(93000).received_at_ms == 92000U);
  } else if (scenario == "burst_evidence") {
    queue(35112, extPacket(1)); queue(35115, grassPacket(1));
    ext::tick(1000); grass::tick(1000);
    for (unsigned i = 4; i <= 6; ++i) {
      queue(35112, extPacket(i)); queue(35115, grassPacket(i));
    }
    ext::tick(91000); grass::tick(91000);
    assert(ext::snapshot(91000).receive_gap_ms == 90000U);
    assert(ext::snapshot(91000).missed_frames == 2U);
    assert(grass::snapshot(91000).receive_gap_ms == 90000U);
    assert(grass::snapshot(91000).missed_frames == 2U);
    assert(grass::snapshot(91000).sequence == 6U);
  } else if (scenario == "grass_malformed") {
    queue(35115, std::array<std::uint8_t, 100>{});
    queue(35115, grassPacket(1)); grass::tick(1000);
    assert(grass::snapshot(1000).fresh);
    assert(grass::receiveCounters().rejected == 1U);
    assert(grass::receiveCounters().accepted == 1U);
  } else if (scenario == "wifi_unavailable") {
    WiFi.connection = 0;
    queue(35112, extPacket(1)); queue(35115, grassPacket(1));
    ext::tick(1000); grass::tick(1000);
    assert(!ext::snapshot(1000).has_packet && !grass::snapshot(1000).has_packet);
    assert(ext::receiveCounters().received == 0U);
    WiFi.connection = WL_CONNECTED;
    ext::tick(2000); grass::tick(2000);
    assert(ext::snapshot(2000).fresh && grass::snapshot(2000).fresh);
  } else if (scenario == "late_packet") {
    grass::ConnectivityTracker tracker{};
    queue(35115, grassPacket(1)); grass::tick(1000);
    grass::observeConnectivity(&tracker, grass::snapshot(1000));
    assert(!grass::snapshot(76001).fresh);
    // During another device's synchronous notification, a new sample arrives.
    queue(35115, grassPacket(4));
    grass::tick(91000);
    assert(grass::observeConnectivity(&tracker, grass::snapshot(91000)) == grass::ConnectivityEvent::none);
  } else { return 2; }
  std::printf("PASS %s\n", scenario.c_str());
}
