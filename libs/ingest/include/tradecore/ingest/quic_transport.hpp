#pragma once

#include <functional>
#include <string>

#include "tradecore/ingest/frame.hpp"

namespace tradecore {
namespace ingest {

class QuicTransport {
 public:
  using FrameCallback = std::function<void(const Frame&)>;

  void start(std::string endpoint_uri, FrameCallback callback);
  void stop();

 private:
  FrameCallback callback_{};
  std::string endpoint_{};
};

}  // namespace ingest
}  // namespace tradecore
