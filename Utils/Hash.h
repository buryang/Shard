#pragma once
#include "Utils/CommonUtils.h"
//#include "Utils/FileArchive.h"
#include <ThirdParty/CSHA/SHA1.h>
#include <folly/Hash.h>
#include <blake3.h>

namespace Shard::Utils {

    template<typename Hash, uint32_t hash_size=20*8>
    class HashSignature final{
    public:
        enum {
            MAX_HASH_SIZE = hash_size,
            MAX_HASH_STR_SIZE = hash_size*2,
        };
        //using ValType = HashType;
        HashSignature() = default;
        HashSignature& FromString(const String& str);
        HashSignature& FromWString(const WString& wstr);
        HashSignature& FromBinary(const Span<uint8_t>& hash);
        String ToString()const;
        WString ToWString()const;
        constexpr bool IsZero()const;
        FORCE_INLINE uint8_t* GetBytes() { return hash_; }
        FORCE_INLINE static constexpr uint32_t GetHashSize() { return MAX_HASH_SIZE; }
        FORCE_INLINE static const HashSignature& Zero() { static HashSignature zero; return zero; }
        FORCE_INLINE constexpr bool operator==(const HashSignature& rhs) const {
            auto ret = std::memcmp(hash_, rhs.hash_, MAX_HASH_SIZE/8);
            return !ret;
        }
        FORCE_INLINE constexpr bool operator!=(const HashSignature& rhs) const {
            return !(*this == rhs);
        }
        FORCE_INLINE operator Integer auto () const { return *reinterpret_cast<Integer auto*>(hash_); }
        //friend void operator << (IOArchive& ar, const HashSignature& hash) {
        //    ar.Serialize(hash.hash_, MAX_HASH_SIZE / 8);
        //}
    private:
        alignas(size_type) uint8_t hash_[MAX_HASH_SIZE / 8] = { 0u };
    };

    //hash conainters
    //my default non-cryptographic hash
    using SpookyV2Hash32 = HashSignature<folly::hash::SpookyHashV2, 32>;
    using SpookyV2Hash64 = HashSignature<folly::hash::SpookyHashV2, 64>;
    using SpookyV2Hash128 = HashSignature<folly::hash::SpookyHashV2, 128>;
    //bad avalanche effect
    //using SHA1Hash = HashSignature<CSHA1, 20*8>; //20 bytes
    //my default cryptographic hash 
    using Blake3Hash64 = HashSignature<blake3_hasher, 64>; //64 bits
    using Blake3Hash256 = HashSignature<blake3_hasher, 32*8>;//32 bytes

    //fnv-1a 64bit hash
    using FNV1AHash64 = HashSignature<struct FNV1_, 64>;

    //template function implement
    template<typename TCHAR>
    static FORCE_INLINE constexpr std::enable_if_t<sizeof(TCHAR) != 1, uint8_t> TCharToNibble(const TCHAR hex) {
        if (hex >= L'0' && hex <= L'9') {
            return hex - L'0';
        }
        if (hex >= L'a' && hex <= L'f') {
            return hex - L'a' + 10;
        }
        if (hex >= L'A' && hex <= L'F') {
            return hex - L'A' + 10;
        }
        return 0;
    }

    template<typename TCHAR>
    static FORCE_INLINE constexpr std::enable_if_t<sizeof(TCHAR) == 1, uint8_t> TCharToNibble(const TCHAR hex) {
        if (hex >= '0' && hex <= '9') {
            return hex - '0';
        }
        if (hex >= 'a' && hex <= 'f') {
            return hex - 'a' + 10;
        }
        if (hex >= 'A' && hex <= L'F') {
            return hex - 'A' + 10;
        }
        return 0;
    }

    template<typename TCHAR>
    static FORCE_INLINE constexpr std::enable_if_t<sizeof(TCHAR) != 1, TCHAR> NibbleToTChar(const uint8_t num) {
        if (num > 9) {
            return L'A' + TCHAR(num - 10);
        }
        return L'0' + TCHAR(num);
    }

    template<typename TCHAR>
    static FORCE_INLINE constexpr std::enable_if_t<sizeof(TCHAR) == 1, TCHAR> NibbleToTChar(const uint8_t num) {
        if (num > 9) {
            return 'A' + TCHAR(num - 10);
        }
        return '0' + TCHAR(num);
    }

    template<typename HashType, uint32_t hash_size>
    HashSignature<HashType, hash_size>& HashSignature<HashType, hash_size>::FromString(const String& str)
    {
        assert(str.size() == MAX_HASH_STR_SIZE);
        for (auto n = 0; n < MAX_HASH_STR_SIZE; n += 2) {
            auto byte = TCharToNibble(str[n]);
            byte <<= 4;
            byte |= TCharToNibble(str[n + 1]) & 0xF;
            hash_[n >> 1] = byte;
        }
        return *this;
    }

    template<typename HashType, uint32_t hash_size>
    HashSignature<HashType, hash_size>& HashSignature<HashType, hash_size>::FromWString(const WString& wstr)
    {
        assert(wstr.size() == MAX_HASH_STR_SIZE);
        for (auto n = 0; n < MAX_HASH_STR_SIZE; n += 2) {
            auto byte = TCharToNibble(wstr[n]);
            byte <<= 4;
            byte |= TCharToNibble(wstr[n + 1]) & 0xF;
            hash_[n >> 1] = byte;
        }
        return *this;
    }

    template<typename HashType, uint32_t hash_size>
    HashSignature<HashType, hash_size>& HashSignature<HashType, hash_size>::FromBinary(const Span<uint8_t>& hash)
    {
        assert(hash.size() == MAX_HASH_SIZE);
        std::memcpy(hash_, hash.data(), MAX_HASH_SIZE);
        return *this;
    }

    template<typename HashType, uint32_t hash_size>
    String HashSignature<HashType, hash_size>::ToString() const
    {
        String str;
        for (auto n = 0; n < MAX_HASH_SIZE; ++n) {
            auto byte = hash_[n];
            str += NibbleToTChar<String::value_type>(byte >> 4);
            str += NibbleToTChar<String::value_type>(byte & 0xF);
        }
        return str;
    }

    template<typename HashType, uint32_t hash_size>
    WString HashSignature<HashType, hash_size>::ToWString() const
    {
        WString wstr;
        for (auto n = 0; n < MAX_HASH_SIZE; ++n) {
            auto byte = hash_[n];
            wstr += NibbleToTChar<WString::value_type>(byte >> 4);
            wstr += NibbleToTChar<WString::value_type>(byte & 0xF);
        }
        return wstr;
    }

    template<typename HashType, uint32_t hash_size>
    constexpr bool HashSignature<HashType, hash_size>::IsZero() const
    {
        /*
        for (auto n = 0; n < MAX_HASH_SIZE; ++n) {
            if (hash_[n] != 0) {
                return false;
            }
        }
        return true;
        */
        return *this == Zero();
    }

    template<uint32_t hash_size=64>
    auto InternBlake3HashForBytes(const uint8_t* bytes, size_t size)->HashSignature<blake3_hasher, hash_size>
    {
        using HashType = HashSignature<blake3_hasher, hash_size>;
        HashType hash;
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, bytes, size);
        blake3_hasher_finalize(&hasher, hash.GetBytes(), hash_size);
        return hash;
    }

    template<uint32_t hash_size=64>
    auto InternSpookyV2HashForBytes(const uint8_t* bytes, size_t size)->HashSignature<folly::hash::SpookyHashV2, hash_size>
    {
        uint64_t hash0, hash1;
        HashSignature<folly::hash::SpookyHashV2, hash_size> hash;
        folly::hash::SpookyHashV2::Hash128(bytes, size, &hash0, &hash1);
        if constexpr (hash_size == 32u) {
            *(uint32_t*)hash.GetBytes() = (uint32_t)hash0;
        }
        else if constexpr (hash_size == 64u) {
            *(uint64_t*)hash.GetBytes() = hash0;
        }
        else if constexpr (hash_size == 128u) {
            auto* ptr = reinterpret_cast<uint64_t*>hash.GetBytes();
            *ptr++ = hash0;
            *ptr = hash1;
        }
        else {
            static_assert(false, "calc not support spooky hash size");
        }
    }

    //fnv-1a 
    auto InternFNV1AHashForBytes(const uint8_t* bytes, size_t size)->FNV1AHash64
    {
        FNV1AHash64 output;
        uint64_t hash = 14695981039346656037ULL;
        for (size_t i = 0; i < size; ++i) {
            hash ^= static_cast<uint64_t>(bytes[i]);
            hash *= 1099511628211ULL;
        }
        *(uint64_t*)output.GetBytes() = hash;
        return output;
    }

    namespace bloom_details
    {
        //mixer(based wyhash finalizer£©
        inline uint64_t splitmix64(uint64_t z) noexcept {
            z += 0x9e3779b97f4a7c15ull;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            return z ^ (z >> 31);
        }

        //mixer£¨multiply-xorshift style£©
        inline uint64_t mulxorshift(uint64_t x) noexcept {
            x ^= x >> 33;
            x *= 0xff51afd7ed558ccdull;
            x ^= x >> 33;
            x *= 0xc4ceb9fe1a85ec53ull;
            return x ^ (x >> 33);
        }

        // default hash (string / integral suitable£©
        template<typename T>
        concept Hashable = std::convertible_to<T, Span<const std::byte>>;

        template<typename T>
        uint64_t hash1(const T& key, uint64_t seed = 0u) noexcept {
            if constexpr (std::integral<std::remove_cvref_t<T>>) {
                uint64_t v = static_cast<uint64_t>(key) ^ seed;
                return mulxorshift(v);
            }
            else if constexpr (Hashable<T>) {
                auto bytes = std::as_bytes(std::span{ &key, 1 });
                uint64_t h = seed ^ bytes.size();
                for (auto b : bytes) {
                    h ^= static_cast<uint64_t>(b);
                    h *= 0xff51afd7ed558ccdull;
                }
                return mulxorshift(h);
            }
            else {
                // fallback: treat as bytes
                return hash1(std::as_bytes(std::span{ &key, 1 }), seed);
            }
        }
    }

    //bloom filter
    template<size_type ExpectedN, double FP = 0.001>
    class BloomFilter final
    {
        static constexpr double p = FP;
        static constexpr size_type n = ExpectedN;
        // m ¡Ö -n ln p / (ln 2)^2 = n ln(1/p) / (ln 2)^2
        static constexpr double ln_one_over_p = -std::log(p); // ln(1/p) = -ln p
        static constexpr double ln2_squared = std::log(2) * std::log(2);
        static constexpr size_type m = static_cast<size_type>(n * ln_one_over_p / ln2_squared + 0.5); // +0.5 for rounding

        static constexpr size_type num_bits = Utils::AlignUp(m, 64u);
        static constexpr size_type num_words = num_bits / 64;

        // k ¡Ö (m/n) ln 2
        static constexpr uint32_t K = static_cast<uint32_t>((static_cast<double>(num_bits) / n) * std::log(2) + 0.5);

        // we only do twice hashing, then double hashing to generate K position
        static constexpr uint32_t K_hash = 2u;

        alignas(size_type) uint64_t bits_[num_words]{};

    public:
        void Clear() noexcept {
            std::memset(bits_, 0u, sizeof(bits_));
        }

        BloomFilter& Insert(const auto& key) noexcept {
            const uint64_t h1 = bloom_details::hash1(key, 0x517cc1b727220a95ull);
            const uint64_t h2 = bloom_details::hash1(key, 0x9e3779b97f4a7c15ull);

            for (unsigned i = 0; i < K; ++i) {
                size_t idx = GetIndex(h1, h2, i);
                size_t word = idx / 64;
                size_t bit = idx % 64;
                bits_[word] |= (uint64_t(1) << bit);
            }
            return *this;
        }

        // test whether the key is contained (may have false positive)
        bool MayContain(const auto& key) const noexcept {
            const uint64_t h1 = bloom_details::hash1(key, 0x517cc1b727220a95ull);
            const uint64_t h2 = bloom_details::hash1(key, 0x9e3779b97f4a7c15ull);

            for (unsigned i = 0; i < K; ++i) {
                size_t idx = GetIndex(h1, h2, i);
                size_t word = idx / 64;
                size_t bit = idx % 64;
                if ((bits_[word] & (uint64_t(1) << bit)) == 0) {
                    return false;
                }
            }
            return true;
        }

        bool DefinitelyNotContain(const auto& key) const noexcept {
            return !MayContain(key);
        }

        constexpr uint32_t HashCount() const noexcept {
            return K;
        }

        double TheoreticalFP() const noexcept {
            return std::pow(1.0 - std::exp(-K * n / double(num_bits)), K);
        }

        // returns true if this filter might contain all elements of the other (query) filter
        bool MayMatch(const BloomFilter& query) const noexcept {
            for (size_t i = 0; i < num_words; ++i) {
                if ((bits_[i] & query.bits_[i]) != query.bits_[i]) {
                    return false;
                }
            }
            return true;
        }

    protected:
        //double hashing: h2(key,i) = (h1(key) + i * h2(key)) & mask
        //but to avoid the expensive%, we make m as close as possible 
        //to a power of 2, or directly use the high - order bits of the multiplication.
        //Here we simply use & (m_bits - 1), but m_bits is not necessarily a power of 2,
        //so we make a compromise.
        inline size_t GetIndex(uint64_t h1, uint64_t h2, unsigned i) const noexcept {
            uint64_t combined = h1 + static_cast<uint64_t>(i) * h2;
            return combined % num_bits;
        }
    };
}