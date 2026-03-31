#include "lpc_speech_banks.hh"
#include "lpc_speech_synth_words.h"
#include "plaits/dsp/speech/lpc_speech_synth.h"
#include <cstdint>

namespace plaits {

class BitStream {
public:
  constexpr BitStream(const uint8_t *p) : p_{p} {}

  constexpr void Flush() {
    while (available_) {
      GetBits(1);
    }
  }

  constexpr uint8_t GetBits(int num_bits) {
    int shift = num_bits;
    if (num_bits > available_) {
      bits_ <<= available_;
      shift -= available_;
      bits_ |= Reverse(*p_++);
      available_ += 8;
    }
    bits_ <<= shift;
    uint8_t result = bits_ >> 8;
    bits_ &= 0xff;
    available_ -= num_bits;
    return result;
  }

  constexpr const uint8_t *ptr() const { return p_; }

private:
  constexpr uint8_t Reverse(uint8_t b) const {
    b = (b >> 4) | (b << 4);
    b = ((b & 0xcc) >> 2) | ((b & 0x33) << 2);
    b = ((b & 0xaa) >> 1) | ((b & 0x55) << 1);
    return b;
  }

  const uint8_t *p_;
  int available_{};
  uint16_t bits_{};
};

static constexpr uint8_t energy_lut_[16] = {0x00, 0x02, 0x03, 0x04, 0x05, 0x07,
                                            0x0a, 0x0f, 0x14, 0x20, 0x29, 0x39,
                                            0x51, 0x72, 0xa1, 0xff};

static constexpr uint8_t period_lut_[64] = {
    0,   16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26, 27,
    28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39, 40,
    41,  42,  43,  45,  47,  49,  51,  53,  54,  57,  59,  61, 63,
    66,  69,  71,  73,  77,  79,  81,  85,  87,  92,  95,  99, 102,
    106, 110, 115, 119, 123, 128, 133, 138, 143, 149, 154, 160};

static constexpr int16_t k0_lut_[32] = {
    -32064, -31872, -31808, -31680, -31552, -31424, -31232, -30848,
    -30592, -30336, -30016, -29696, -29376, -28928, -28480, -27968,
    -26368, -24256, -21632, -18368, -14528, -10048, -5184,  0,
    5184,   10048,  14528,  18368,  21632,  24256,  26368,  27968};

static constexpr int16_t k1_lut_[32] = {
    -20992, -19328, -17536, -15552, -13440, -11200, -8768, -6272,
    -3712,  -1088,  1536,   4160,   6720,   9216,   11584, 13824,
    15936,  17856,  19648,  21248,  22656,  24000,  25152, 26176,
    27072,  27840,  28544,  29120,  29632,  30080,  30464, 32384};

static constexpr int8_t k2_lut_[16] = {-110, -97, -83, -70, -56, -43, -29, -16,
                                       -2,   11,  25,  38,  52,  65,  79,  92};

static constexpr int8_t k3_lut_[16] = {-82, -68, -54, -40, -26, -12, 1,   15,
                                       29,  43,  57,  71,  85,  99,  113, 126};

static constexpr int8_t k4_lut_[16] = {-82, -70, -59, -47, -35, -24, -12, -1,
                                       11,  23,  34,  46,  57,  69,  81,  92};

static constexpr int8_t k5_lut_[16] = {-64, -53, -42, -31, -20, -9, 3,  14,
                                       25,  36,  47,  58,  69,  80, 91, 102};

static constexpr int8_t k6_lut_[16] = {-77, -65, -53, -41, -29, -17, -5, 7,
                                       19,  31,  43,  55,  67,  79,  90, 102};

static constexpr int8_t k7_lut_[8] = {-64, -40, -16, 7, 31, 55, 79, 102};

static constexpr int8_t k8_lut_[8] = {-64, -44, -24, -4, 16, 37, 57, 77};

static constexpr int8_t k9_lut_[8] = {-51, -33, -15, 4, 22, 32, 59, 77};

static constexpr auto calc_word_num_frames(std::span<const uint8_t> w) {
  BitStream bitstream{w.data()};
  auto out = 0u;

  while (true) {
    int energy = bitstream.GetBits(4);
    if (energy == 0) {
    } else if (energy == 0xf) {
      bitstream.Flush();
      break;
    } else {
      const auto repeat = bitstream.GetBits(1);
      const auto period = period_lut_[bitstream.GetBits(6)];
      if (!repeat) {
        bitstream.GetBits(5);
        bitstream.GetBits(5);
        bitstream.GetBits(4);
        bitstream.GetBits(4);
        if (period) {
          bitstream.GetBits(4);
          bitstream.GetBits(4);
          bitstream.GetBits(4);
          bitstream.GetBits(3);
          bitstream.GetBits(3);
          bitstream.GetBits(3);
        }
      }
    }
    ++out;
  }
  return out;
};

template <uint32_t size>
static constexpr auto parse_word(const std::span<const uint8_t> w) {
  BitStream bitstream{w.data()};

  std::array<plaits::LPCSpeechSynth::Frame, size> out = {};
  auto cnt = 0u;

  plaits::LPCSpeechSynth::Frame frame{};

  while (true) {
    int energy = bitstream.GetBits(4);
    if (energy == 0) {
      frame.energy = 0;
    } else if (energy == 0xf) {
      bitstream.Flush();
      break;
    } else {
      frame.energy = energy_lut_[energy];
      bool repeat = bitstream.GetBits(1);
      frame.period = period_lut_[bitstream.GetBits(6)];
      if (!repeat) {
        frame.k0 = k0_lut_[bitstream.GetBits(5)];
        frame.k1 = k1_lut_[bitstream.GetBits(5)];
        frame.k2 = k2_lut_[bitstream.GetBits(4)];
        frame.k3 = k3_lut_[bitstream.GetBits(4)];
        if (frame.period) {
          frame.k4 = k4_lut_[bitstream.GetBits(4)];
          frame.k5 = k5_lut_[bitstream.GetBits(4)];
          frame.k6 = k6_lut_[bitstream.GetBits(4)];
          frame.k7 = k7_lut_[bitstream.GetBits(3)];
          frame.k8 = k8_lut_[bitstream.GetBits(3)];
          frame.k9 = k9_lut_[bitstream.GetBits(3)];
        }
      }
    }
    out[cnt++] = frame;
  }

  return out;
};

template <auto &T> constexpr auto parse_word() {
  return parse_word<calc_word_num_frames(T) + 1>(T);
};

namespace Frames {
namespace Alphabet {

static constexpr auto a = parse_word<LPCWords::Alphabet::a>();
static constexpr auto b = parse_word<LPCWords::Alphabet::b>();
static constexpr auto c = parse_word<LPCWords::Alphabet::c>();
static constexpr auto d = parse_word<LPCWords::Alphabet::d>();
static constexpr auto e = parse_word<LPCWords::Alphabet::e>();
static constexpr auto f = parse_word<LPCWords::Alphabet::f>();
static constexpr auto g = parse_word<LPCWords::Alphabet::g>();
static constexpr auto h = parse_word<LPCWords::Alphabet::h>();
static constexpr auto i = parse_word<LPCWords::Alphabet::i>();
static constexpr auto j = parse_word<LPCWords::Alphabet::j>();
static constexpr auto k = parse_word<LPCWords::Alphabet::k>();
static constexpr auto l = parse_word<LPCWords::Alphabet::l>();
static constexpr auto m = parse_word<LPCWords::Alphabet::m>();
static constexpr auto n = parse_word<LPCWords::Alphabet::n>();
static constexpr auto o = parse_word<LPCWords::Alphabet::o>();
static constexpr auto p = parse_word<LPCWords::Alphabet::p>();
static constexpr auto q = parse_word<LPCWords::Alphabet::q>();
static constexpr auto r = parse_word<LPCWords::Alphabet::r>();
static constexpr auto s = parse_word<LPCWords::Alphabet::s>();
static constexpr auto t = parse_word<LPCWords::Alphabet::t>();
static constexpr auto u = parse_word<LPCWords::Alphabet::u>();
static constexpr auto v = parse_word<LPCWords::Alphabet::v>();
static constexpr auto w = parse_word<LPCWords::Alphabet::w>();
static constexpr auto x = parse_word<LPCWords::Alphabet::x>();
static constexpr auto y = parse_word<LPCWords::Alphabet::y>();
static constexpr auto z = parse_word<LPCWords::Alphabet::z>();

static constexpr auto alphabet = std::array{
    FrameSpan{a}, FrameSpan{b}, FrameSpan{c}, FrameSpan{d}, FrameSpan{e},
    FrameSpan{f}, FrameSpan{g}, FrameSpan{h}, FrameSpan{i}, FrameSpan{j},
    FrameSpan{k}, FrameSpan{l}, FrameSpan{m}, FrameSpan{n}, FrameSpan{o},
    FrameSpan{p}, FrameSpan{q}, FrameSpan{r}, FrameSpan{s}, FrameSpan{t},
    FrameSpan{u}, FrameSpan{v}, FrameSpan{w}, FrameSpan{x}, FrameSpan{y},
    FrameSpan{z},
};
} // namespace Alphabet

namespace Nato {
static constexpr auto a = parse_word<LPCWords::Nato::a>();
static constexpr auto b = parse_word<LPCWords::Nato::b>();
static constexpr auto c = parse_word<LPCWords::Nato::c>();
static constexpr auto d = parse_word<LPCWords::Nato::d>();
static constexpr auto e = parse_word<LPCWords::Nato::e>();
static constexpr auto f = parse_word<LPCWords::Nato::f>();
static constexpr auto g = parse_word<LPCWords::Nato::g>();
static constexpr auto h = parse_word<LPCWords::Nato::h>();
static constexpr auto i = parse_word<LPCWords::Nato::i>();
static constexpr auto j = parse_word<LPCWords::Nato::j>();
static constexpr auto k = parse_word<LPCWords::Nato::k>();
static constexpr auto l = parse_word<LPCWords::Nato::l>();
static constexpr auto m = parse_word<LPCWords::Nato::m>();
static constexpr auto n = parse_word<LPCWords::Nato::n>();
static constexpr auto o = parse_word<LPCWords::Nato::o>();
static constexpr auto p = parse_word<LPCWords::Nato::p>();
static constexpr auto q = parse_word<LPCWords::Nato::q>();
static constexpr auto r = parse_word<LPCWords::Nato::r>();
static constexpr auto s = parse_word<LPCWords::Nato::s>();
static constexpr auto t = parse_word<LPCWords::Nato::t>();
static constexpr auto u = parse_word<LPCWords::Nato::u>();
static constexpr auto v = parse_word<LPCWords::Nato::v>();
static constexpr auto w = parse_word<LPCWords::Nato::w>();
static constexpr auto x = parse_word<LPCWords::Nato::x>();
static constexpr auto y = parse_word<LPCWords::Nato::y>();
static constexpr auto z = parse_word<LPCWords::Nato::z>();

static constexpr auto nato = std::array{
    FrameSpan{a}, FrameSpan{b}, FrameSpan{c}, FrameSpan{d}, FrameSpan{e},
    FrameSpan{f}, FrameSpan{g}, FrameSpan{h}, FrameSpan{i}, FrameSpan{j},
    FrameSpan{k}, FrameSpan{l}, FrameSpan{m}, FrameSpan{n}, FrameSpan{o},
    FrameSpan{p}, FrameSpan{q}, FrameSpan{r}, FrameSpan{s}, FrameSpan{t},
    FrameSpan{u}, FrameSpan{v}, FrameSpan{w}, FrameSpan{x}, FrameSpan{y},
    FrameSpan{z},
};

} // namespace Nato

namespace Number {
static constexpr auto one = parse_word<LPCWords::Numbers::one>();
static constexpr auto two = parse_word<LPCWords::Numbers::two>();
static constexpr auto three = parse_word<LPCWords::Numbers::three>();
static constexpr auto four = parse_word<LPCWords::Numbers::four>();
static constexpr auto five = parse_word<LPCWords::Numbers::five>();
static constexpr auto six = parse_word<LPCWords::Numbers::six>();
static constexpr auto seven = parse_word<LPCWords::Numbers::seven>();
static constexpr auto eight = parse_word<LPCWords::Numbers::eight>();
static constexpr auto nine = parse_word<LPCWords::Numbers::nine>();
static constexpr auto ten = parse_word<LPCWords::Numbers::ten>();

static constexpr auto number = std::array{
    FrameSpan{one},  FrameSpan{two}, FrameSpan{three}, FrameSpan{four},
    FrameSpan{five}, FrameSpan{six}, FrameSpan{seven}, FrameSpan{eight},
    FrameSpan{nine}, FrameSpan{ten},
};
} // namespace Number

namespace Color {
static constexpr auto red = parse_word<LPCWords::Colors::red>();
static constexpr auto orange = parse_word<LPCWords::Colors::orange>();
static constexpr auto yellow = parse_word<LPCWords::Colors::yellow>();
static constexpr auto green = parse_word<LPCWords::Colors::green>();
static constexpr auto blue = parse_word<LPCWords::Colors::blue>();
static constexpr auto indigo = parse_word<LPCWords::Colors::indigo>();
static constexpr auto violet = parse_word<LPCWords::Colors::violet>();

static constexpr auto color = std::array{
    FrameSpan{red},  FrameSpan{orange}, FrameSpan{yellow}, FrameSpan{green},
    FrameSpan{blue}, FrameSpan{indigo}, FrameSpan{violet},
};
} // namespace Color

} // namespace Frames

constexpr std::array<std::span<const FrameSpan>, 4> bank = {
    std::span<const FrameSpan>{Frames::Color::color},
    std::span<const FrameSpan>{Frames::Number::number},
    std::span<const FrameSpan>{Frames::Alphabet::alphabet},
    std::span<const FrameSpan>{Frames::Nato::nato},
};
} // namespace plaits
