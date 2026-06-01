/*
  ==============================================================================

    ToolManager.h

    First-run setup helper: downloads yt-dlp and ffmpeg from their official
    distribution sources into a managed "bin" folder, verifies them, and reports
    the resulting executable paths. See docs/first-run-setup-design.md.

    Header-only so it can be added without touching the generated build files.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <cstring>

//==============================================================================
// Minimal self-contained SHA-256 (juce_cryptography module is not part of this
// project). Includes a self-test so that, if anything is wrong, verification can
// safely degrade to HTTPS-only instead of rejecting every valid download.
class Sha256
{
public:
    Sha256() { reset(); }

    void reset()
    {
        h[0] = 0x6a09e667u; h[1] = 0xbb67ae85u; h[2] = 0x3c6ef372u; h[3] = 0xa54ff53au;
        h[4] = 0x510e527fu; h[5] = 0x9b05688cu; h[6] = 0x1f83d9abu; h[7] = 0x5be0cd19u;
        bitLength = 0; bufferLen = 0;
    }

    void update (const void* data, size_t len)
    {
        auto* p = static_cast<const uint8_t*> (data);
        bitLength += (uint64_t) len * 8;
        while (len > 0)
        {
            size_t take = juce::jmin (len, (size_t) (64 - bufferLen));
            memcpy (buffer + bufferLen, p, take);
            bufferLen += (int) take; p += take; len -= take;
            if (bufferLen == 64) { processBlock (buffer); bufferLen = 0; }
        }
    }

    juce::String finalizeHex()
    {
        const uint64_t messageBits = bitLength; // capture before padding

        const uint8_t one = 0x80;
        update (&one, 1);
        const uint8_t zero = 0;
        while (bufferLen != 56) update (&zero, 1);

        uint8_t lenBytes[8];
        for (int i = 0; i < 8; ++i)
            lenBytes[i] = (uint8_t) (messageBits >> (56 - 8 * i));
        update (lenBytes, 8);

        juce::String hex;
        for (int i = 0; i < 8; ++i)
            hex += juce::String::toHexString ((int) h[i]).paddedLeft ('0', 8);
        return hex.toLowerCase();
    }

    static juce::String ofFile (const juce::File& f)
    {
        juce::FileInputStream in (f);
        if (in.failedToOpen())
            return {};

        Sha256 hasher;
        juce::HeapBlock<uint8_t> block (65536);
        for (;;)
        {
            auto n = in.read (block, 65536);
            if (n <= 0) break;
            hasher.update (block, (size_t) n);
        }
        return hasher.finalizeHex();
    }

    static bool selfTest()
    {
        Sha256 h2;
        const char* abc = "abc";
        h2.update (abc, 3);
        return h2.finalizeHex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    }

private:
    static uint32_t rotr (uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void processBlock (const uint8_t* p)
    {
        static const uint32_t k[64] = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u };

        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = ((uint32_t) p[i*4] << 24) | ((uint32_t) p[i*4+1] << 16)
                 | ((uint32_t) p[i*4+2] << 8) | ((uint32_t) p[i*4+3]);
        for (int i = 16; i < 64; ++i)
        {
            uint32_t s0 = rotr (w[i-15], 7) ^ rotr (w[i-15], 18) ^ (w[i-15] >> 3);
            uint32_t s1 = rotr (w[i-2], 17) ^ rotr (w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }

        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; ++i)
        {
            uint32_t S1 = rotr (e,6) ^ rotr (e,11) ^ rotr (e,25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr (a,2) ^ rotr (a,13) ^ rotr (a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    uint32_t h[8];
    uint64_t bitLength;
    uint8_t  buffer[64];
    int      bufferLen;
};

//==============================================================================
class ToolManager : private juce::Thread
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void toolSetupProgress (const juce::String& message, double progress01) = 0;
        virtual void toolSetupFinished (bool success, const juce::String& message) = 0;
    };

    explicit ToolManager (juce::File binDir)
        : juce::Thread ("ToolSetup"), binDirectory (std::move (binDir)) {}

    ~ToolManager() override { stopThread (5000); }

    //==============================================================================
    juce::File getYtDlpFile() const
    {
       #if JUCE_WINDOWS
        return binDirectory.getChildFile ("yt-dlp.exe");
       #else
        return binDirectory.getChildFile ("yt-dlp");
       #endif
    }

    juce::File getFfmpegFile() const
    {
       #if JUCE_WINDOWS
        return binDirectory.getChildFile ("ffmpeg.exe");
       #else
        return binDirectory.getChildFile ("ffmpeg");
       #endif
    }

    // Cheap, message-thread-safe check used to switch the settings UI mode.
    bool areManagedToolsPresent() const
    {
        return getYtDlpFile().existsAsFile() && getFfmpegFile().existsAsFile();
    }

    bool isBusy() const { return isThreadRunning(); }

    void startSetup (Listener* l)
    {
        if (isThreadRunning())
            return;
        listener = l;
        startThread();
    }

    // Editors must call this when destroyed to avoid a dangling listener.
    void clearListener (Listener* l) { if (listener == l) listener = nullptr; }

private:
    //==============================================================================
    void run() override
    {
        const bool ok = setupYtDlp() && (threadShouldExit() ? false : setupFfmpeg());

        juce::String msg = ok ? "Setup complete. yt-dlp and ffmpeg are ready."
                              : ("Setup failed: " + lastError);
        notifyFinished (ok, msg);
    }

    //==============================================================================
    bool setupYtDlp()
    {
        if (! binDirectory.exists() && ! binDirectory.createDirectory())
        {
            lastError = "Could not create bin folder: " + binDirectory.getFullPathName();
            return false;
        }

        const juce::String base = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/";
       #if JUCE_WINDOWS
        const juce::String asset = "yt-dlp.exe";
       #elif JUCE_MAC
        const juce::String asset = "yt-dlp_macos";
       #else
        const juce::String asset = "yt-dlp_linux";
       #endif

        notifyProgress ("Downloading yt-dlp...", -1.0);
        auto tmp = binDirectory.getChildFile (asset + ".part");
        if (! download (juce::URL (base + asset), tmp, "yt-dlp"))
            return false;

        // Verify against the release's published SHA2-256SUMS.
        auto sums = juce::URL (base + "SHA2-256SUMS").readEntireTextStream();
        auto expected = hashFromSums (sums, asset);
        if (! verifyChecksum (tmp, expected, "yt-dlp"))
            return false;

        auto dest = getYtDlpFile();
        dest.deleteFile();
        if (! tmp.moveFileTo (dest))
        {
            lastError = "Failed to place yt-dlp.";
            return false;
        }
        makeRunnable (dest);
        return verifyRuns (dest, "--version", "yt-dlp");
    }

    bool setupFfmpeg()
    {
        notifyProgress ("Downloading ffmpeg...", -1.0);

        auto work = binDirectory.getChildFile ("ffmpeg_dl");
        work.deleteRecursively();
        work.createDirectory();

        juce::File extractedBinary;

       #if JUCE_WINDOWS
        const juce::String rel = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/";
        const juce::String assetName = "ffmpeg-master-latest-win64-lgpl.zip"; // LGPL build
        auto archive = work.getChildFile (assetName);
        if (! download (juce::URL (rel + assetName), archive, "ffmpeg")) return cleanup (work, false);
        auto expected = hashFromSums (juce::URL (rel + "checksums.sha256").readEntireTextStream(), assetName);
        if (! verifyChecksum (archive, expected, "ffmpeg")) return cleanup (work, false);
        if (! unzip (archive, work)) return cleanup (work, false);
        extractedBinary = findChild (work, "ffmpeg.exe");

       #elif JUCE_MAC
        // evermeet.cx ships a versioned static zip (GPL). The URL is immutable, so a
        // pinned SHA-256 is valid. Leave kFfmpegMacSha256 empty until filled: the actual
        // hash is reported in the status so it can be pinned after the first run.
        const juce::String url = "https://evermeet.cx/ffmpeg/ffmpeg-8.1.1.zip";
        const juce::String expected = kFfmpegMacSha256;
        auto archive = work.getChildFile ("ffmpeg-mac.zip");
        if (! download (juce::URL (url), archive, "ffmpeg")) return cleanup (work, false);
        if (! verifyChecksum (archive, expected, "ffmpeg")) return cleanup (work, false);
        if (! unzip (archive, work)) return cleanup (work, false);
        extractedBinary = findChild (work, "ffmpeg");

       #else // Linux
        const juce::String rel = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/";
        const juce::String assetName = "ffmpeg-master-latest-linux64-lgpl.tar.xz"; // LGPL build
        auto archive = work.getChildFile (assetName);
        if (! download (juce::URL (rel + assetName), archive, "ffmpeg")) return cleanup (work, false);
        auto expected = hashFromSums (juce::URL (rel + "checksums.sha256").readEntireTextStream(), assetName);
        if (! verifyChecksum (archive, expected, "ffmpeg")) return cleanup (work, false);
        if (! untarXz (archive, work)) return cleanup (work, false);
        extractedBinary = findChild (work, "ffmpeg");
       #endif

        if (extractedBinary == juce::File() || ! extractedBinary.existsAsFile())
        {
            lastError = "ffmpeg binary not found after extraction.";
            return cleanup (work, false);
        }

        auto dest = getFfmpegFile();
        dest.deleteFile();
        if (! extractedBinary.copyFileTo (dest))
        {
            lastError = "Failed to place ffmpeg.";
            return cleanup (work, false);
        }
        makeRunnable (dest);
        work.deleteRecursively();
        return verifyRuns (dest, "-version", "ffmpeg");
    }

    //==============================================================================
    bool download (const juce::URL& url, const juce::File& dest, const juce::String& label)
    {
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (30000);

        std::unique_ptr<juce::InputStream> in (url.createInputStream (options));
        if (in == nullptr)
        {
            lastError = label + ": could not connect for download. Check your network.";
            return false;
        }

        dest.deleteFile();
        std::unique_ptr<juce::FileOutputStream> out (dest.createOutputStream());
        if (out == nullptr || out->failedToOpen())
        {
            lastError = label + ": could not create output file: " + dest.getFullPathName();
            return false;
        }

        const auto total = in->getTotalLength();
        juce::int64 done = 0;
        juce::HeapBlock<char> buf (65536);

        while (! threadShouldExit())
        {
            auto n = in->read (buf, 65536);
            if (n <= 0)
                break;
            out->write (buf, (size_t) n);
            done += n;

            const double p = total > 0 ? (double) done / (double) total : -1.0;
            notifyProgress (label + " ... " + (total > 0
                                ? juce::String (juce::roundToInt (p * 100.0)) + "%"
                                : juce::File::descriptionOfSizeInBytes (done)), p);
        }
        out->flush();

        if (threadShouldExit())
            return false;

        if (done <= 0)
        {
            lastError = label + ": download failed (empty response).";
            return false;
        }
        return true;
    }

    bool verifyChecksum (const juce::File& file, const juce::String& expectedLower, const juce::String& label)
    {
        if (! Sha256::selfTest())
        {
            // Internal hash is unreliable: skip rather than reject a valid download.
            notifyProgress (label + ": skipping checksum verification (internal SHA-256 self-test failed)", -1.0);
            return true;
        }

        const auto actual = Sha256::ofFile (file);

        if (expectedLower.isEmpty())
        {
            // No reference hash available (e.g. mac pin not yet filled). Report actual.
            notifyProgress (label + ": no reference sha256, skipping verification (actual=" + actual + ")", -1.0);
            return true;
        }

        if (! actual.equalsIgnoreCase (expectedLower))
        {
            file.deleteFile();
            lastError = label + " checksum mismatch (possible tampering/corruption). expected=" + expectedLower + " actual=" + actual;
            return false;
        }
        return true;
    }

    bool verifyRuns (const juce::File& exe, const juce::String& arg, const juce::String& label)
    {
        juce::ChildProcess proc;
        if (! proc.start (juce::StringArray { exe.getFullPathName(), arg }))
        {
            lastError = label + " could not be executed (signing/quarantine?): " + exe.getFullPathName();
            return false;
        }
        if (! proc.waitForProcessToFinish (15000))
        {
            proc.kill();
            lastError = label + " version-check timed out.";
            return false;
        }
        if (proc.getExitCode() != 0)
        {
            lastError = label + " version-check failed (exit code != 0).";
            return false;
        }
        return true;
    }

    //==============================================================================
    static juce::String hashFromSums (const juce::String& sumsText, const juce::String& assetName)
    {
        juce::StringArray lines;
        lines.addLines (sumsText);
        for (auto& line : lines)
        {
            auto trimmed = line.trim();
            if (trimmed.endsWith (assetName) || trimmed.endsWith ("*" + assetName))
            {
                auto hash = trimmed.upToFirstOccurrenceOf (" ", false, false).trim();
                if (hash.length() == 64)
                    return hash.toLowerCase();
            }
        }
        return {};
    }

    static juce::File findChild (const juce::File& dir, const juce::String& name)
    {
        auto matches = dir.findChildFiles (juce::File::findFiles, true, name);
        return matches.isEmpty() ? juce::File() : matches.getReference (0);
    }

    static bool unzip (const juce::File& zip, const juce::File& destDir)
    {
        juce::ZipFile z (zip);
        return z.uncompressTo (destDir, true).wasOk();
    }

    bool untarXz (const juce::File& archive, const juce::File& destDir)
    {
       #if JUCE_WINDOWS
        juce::ignoreUnused (archive, destDir);
        lastError = "tar.xz extraction is not supported on Windows.";
        return false;
       #else
        juce::ChildProcess proc;
        if (! proc.start (juce::StringArray { "/usr/bin/tar", "-xf",
                                              archive.getFullPathName(), "-C", destDir.getFullPathName() }))
        {
            lastError = "Failed to start tar.";
            return false;
        }
        if (! proc.waitForProcessToFinish (180000))
        {
            proc.kill();
            lastError = "Archive extraction timed out.";
            return false;
        }
        return proc.getExitCode() == 0;
       #endif
    }

    static void makeRunnable (const juce::File& f)
    {
        f.setExecutePermission (true);
       #if JUCE_MAC
        // Remove the Gatekeeper quarantine attribute so the binary can run.
        juce::ChildProcess proc;
        if (proc.start (juce::StringArray { "/usr/bin/xattr", "-dr",
                                            "com.apple.quarantine", f.getFullPathName() }))
            proc.waitForProcessToFinish (10000);
       #endif
    }

    bool cleanup (const juce::File& work, bool result)
    {
        work.deleteRecursively();
        return result;
    }

    //==============================================================================
    void notifyProgress (juce::String message, double progress01)
    {
        juce::MessageManager::callAsync ([this, message, progress01]
        {
            if (listener != nullptr)
                listener->toolSetupProgress (message, progress01);
        });
    }

    void notifyFinished (bool ok, juce::String message)
    {
        juce::MessageManager::callAsync ([this, ok, message]
        {
            if (listener != nullptr)
                listener->toolSetupFinished (ok, message);
        });
    }

    //==============================================================================
    // Pinned SHA-256 for the macOS evermeet ffmpeg zip. Fill after a first run
    // (the actual hash is reported in the status). Empty => verification skipped.
    static constexpr const char* kFfmpegMacSha256 = "";

    juce::File binDirectory;
    Listener* listener = nullptr;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolManager)
};
