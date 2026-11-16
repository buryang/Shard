import random
import struct

MASK32 = 0xFFFFFFFF


def asuint(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def asfloat(value):
    return struct.unpack("<f", struct.pack("<I", value & MASK32))[0]


def convert_float_to_uint(value):
    bits = asuint(value)
    mask = ((-(bits >> 31)) & MASK32) | 0x80000000
    return bits ^ mask


def inv_convert_uint_to_float(value):
    mask = (((value >> 31) - 1) & MASK32) | 0x80000000
    return asfloat(value ^ mask)


def convert_int_to_uint(value):
    return (value & MASK32) ^ 0x80000000


def inv_convert_uint_to_int(value):
    return ((value & MASK32) ^ 0x80000000)


def to_int32(value):
    value &= MASK32
    return value - 0x100000000 if value & 0x80000000 else value


def cond_flip_bits(value, flip):
    return value ^ MASK32 if flip else value


def insert_one_bit(value, one_bit_mask):
    mask = one_bit_mask - 1
    return (((value & ~mask) << 1) | (value & mask) | one_bit_mask) & MASK32


def is_swap_needed(lhs, rhs, null_item):
    return (lhs ^ null_item) < (rhs ^ null_item)


def extract_digit(value, shift):
    return (value >> shift) & 0xFF


def extract_packed_index(value, shift):
    return (value >> (shift + 1)) & 0x7F


def extract_packed_shift(value, shift):
    return 16 if (value >> shift) & 1 else 0


def extract_packed_value(packed, value, shift):
    return (packed >> extract_packed_shift(value, shift)) & 0xFFFF


def test_float_transform(rng):
    values = [rng.uniform(-1.0e6, 1.0e6) for _ in range(10000)]
    sorted_values = sorted(values)
    transformed = [convert_float_to_uint(value) for value in sorted_values]
    for value, encoded in zip(sorted_values, transformed):
        restored = inv_convert_uint_to_float(encoded)
        assert asuint(restored) == asuint(value)
    assert transformed == sorted(transformed)


def test_int_transform(rng):
    values = [rng.randint(-0x80000000, 0x7FFFFFFF) for _ in range(10000)]
    sorted_values = sorted(values)
    transformed = [convert_int_to_uint(value) for value in sorted_values]
    for value, encoded in zip(sorted_values, transformed):
        assert to_int32(inv_convert_uint_to_int(encoded)) == value
    assert transformed == sorted(transformed)


def test_bitonic(rng):
    for reverse in (False, True):
        for size in (1, 2, 3, 31, 32, 33, 100, 1024):
            values = [rng.getrandbits(32) for _ in range(size)]
            null_item = 0x00000000 if reverse else MASK32
            keys = [value ^ null_item for value in values]
            for left in range(size):
                for right in range(left + 1, size):
                    if is_swap_needed(keys[left], keys[right], 0):
                        keys[left], keys[right] = keys[right], keys[left]
            plain = [key ^ null_item for key in keys]
            assert plain == sorted(values, reverse=reverse)


def test_bits(rng):
    for _ in range(10000):
        value = rng.getrandbits(32)
        shift = rng.choice((0, 8, 16, 24))
        assert extract_digit(value, shift) == (value >> shift) & 0xFF
        assert extract_packed_index(value, shift) == (value >> (shift + 1)) & 0x7F
        assert extract_packed_shift(value, shift) in (0, 16)
        packed = rng.getrandbits(32)
        assert extract_packed_value(packed, value, shift) == (
            packed >> extract_packed_shift(value, shift)
        ) & 0xFFFF
        bit = 1 << rng.randrange(0, 31)
        inserted = insert_one_bit(value, bit)
        assert inserted & bit
        assert (inserted & (bit - 1)) == (value & (bit - 1))
        assert cond_flip_bits(value, False) == value
        assert cond_flip_bits(value, True) == (value ^ MASK32)


def main():
    rng = random.Random(12345)
    test_float_transform(rng)
    test_int_transform(rng)
    test_bitonic(rng)
    test_bits(rng)
    print("property tests: PASS")


if __name__ == "__main__":
    main()
