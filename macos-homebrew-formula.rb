# Homebrew Formula Template for ka9q-radio
# Copy to homebrew-core or your tap
# Usage: brew install ka9q-radio

class Ka9qRadio < Formula
  desc "Multichannel SDR based on fast convolution and IP multicasting"
  homepage "https://github.com/ka9q/ka9q-radio"
  url "https://github.com/ka9q/ka9q-radio/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "YOUR_SHA256_HERE"
  license "GPL-3.0"

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "fftw"
  depends_on "opus"
  depends_on "libbsd"
  depends_on "iniparser"
  depends_on "libusb"
  depends_on "portaudio"
  depends_on "ncurses"

  # Optional hardware support
  depends_on "airspy" => :optional
  depends_on "hackrf" => :optional
  depends_on "rtl-sdr" => :optional

  def install
    mkdir "build" do
      system "cmake", "..", *std_cmake_args,
                      "-DCMAKE_PREFIX_PATH=#{HOMEBREW_PREFIX}"
      system "cmake", "--build", "."
      system "cmake", "--install", "."
    end

    # Install example configs
    (etc/"radio").mkpath
    (var/"lib/ka9q-radio").mkpath
  end

  def caveats
    <<~EOS
      Configuration files should be placed in:
        #{etc}/radio/

      Example: #{etc}/radio/radiod@hf.conf

      Start radiod manually:
        #{opt_sbin}/radiod -v #{etc}/radio/radiod@hf.conf

      Note: macOS does not support all Linux-specific features (e.g., Avahi).
    EOS
  end

  test do
    system "#{sbin}/radiod", "--help"
    system "#{bin}/control", "--help"
  end
end
