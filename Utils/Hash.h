#pragma once
#include "Utils/CommonUtils.h"
#include <ThirdParty/CSHA/SHA1.h>
#include <folly/Hash.h>

namespace MetaInit::Utils {
	template<typename Hash, uint32_t hash_size=20>
	class HashContainer {
	public:
		enum {
			MAX_HASH_SIZE = hash_size,
			MAX_HASH_STR_SIZE = hash_size *2,
		};
		using ValType = HashType;
		HashContainer& FromString(const String& str);
		HashContainer& FromWString(const WString& wstr);
		HashContainer& FromBinary(const Span<uint8_t>& hash);
		String ToString()const;
		WString ToWString()const;
		constexpr bool IsZero()const;
		FORCE_INLINE uint8_t* GetBytes() { return hash_; }
		FORCE_INLINE uint32_t GetHashSize()const { return size_; }
		FORCE_INLINE constexpr bool operator==(const HashContainer& rhs) const {
			auto ret = std::memcmp(hash_, rhs.hash_, size_);
			return !ret;
		}
		FORCE_INLINE constexpr bool operator!=(const HashContainer& rhs) const {
			return !(*this == rhs);
		}
	private:
		alignas(32) uint8_t hash_[MAX_HASH_SIZE];
	};

	//hash conainters
	using SpookyV2Hash64 = HashContainer<folly::hash::SpookyHashV2, 64>;
	using SpookyV2Hash128 = HashContainer<folly::hash::SpookyHashV2, 128>;
	using SHA1Hash = HashContainer<CSHA1, 20*8>; //20 bytes


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
	HashContainer<HashType, hash_size>& HashContainer<HashType, hash_size>::FromString(const String& str)
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
	HashContainer<HashType, hash_size>& HashContainer<HashType, hash_size>::FromWString(const WString& wstr)
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
	HashContainer<HashType, hash_size>& HashContainer<HashType, hash_size>::FromBinary(const Span<uint8_t>& hash)
	{
		assert(hash.size() == MAX_HASH_SIZE);
		std::memcpy(hash_, hash.data(), MAX_HASH_SIZE);
		return *this;
	}

	template<typename HashType, uint32_t hash_size>
	String HashContainer<HashType, hash_size>::ToString() const
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
	WString HashContainer<HashType, hash_size>::ToWString() const
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
	constexpr bool HashContainer<HashType, hash_size>::IsZero() const
	{
		for (auto n = 0; n < MAX_HASH_SIZE; ++n) {
			if (hash_[n] != 0) {
				return false;
			}
		}
		return true;
	}
}