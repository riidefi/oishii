#pragma once

#include <vector>
#include <tuple>

#include "vector_writer.hxx"
#include "link.hxx"
#include "../util/util.hxx"

namespace oishii {

class Writer : public VectorWriter
{
public:

	template <typename T, EndianSelect E = EndianSelect::Current>
	void transfer(T& out)
	{
		write<T>(out);
	}


	template <typename T, EndianSelect E=EndianSelect::Current>
	void write(T val)
	{
		while (tell() + sizeof(T) > mBuf->size())
			mBuf->push_back(0);

		// Unsafe, but we guaruntee buffer is sized enough above
		*reinterpret_cast<T*>(&(*mBuf)[tell()]) = endianDecode<T, E>(val);

		seek<Whence::Current>(sizeof(T));
	}
	template <EndianSelect E = EndianSelect::Current>
	void writeN(std::size_t sz, u32 val)
	{
		while (tell() + sz > mBuf->size())
			mBuf->push_back(0);

		// Unsafe, but we ensure buffer is sized enough above
		u32 decoded = endianDecode<u32, E>(val);

		for (int i = 0; i < sz; ++i)
			(*mBuf)[tell() + i] = static_cast<u8>(decoded >> (8 * i));

		seek<Whence::Current>(sz);
	}

	std::string mNameSpace = ""; // set by linker, stored in reservations
	std::string mBlockName = ""; // set by linker, stored in reservations

	struct ReferenceEntry
	{
		std::size_t addr; //!< Address in writer stream.
		std::size_t TSize; //!< Size of link type
		Link mLink; //!< The link.

		std::string nameSpace; //!< Namespace
		std::string blockName; //!< Necessary for child namespace lookup
	};
    std::vector<ReferenceEntry> mLinkReservations; // To be resolved by linker

	template <typename T>
    void writeLink(const Link& link)
    {
        // Add our resolvement reservation
		mLinkReservations.push_back({ tell(), sizeof(T), link, mNameSpace, mBlockName }); // copy

		// Dummy data
		write<T>(static_cast<T>(0xcccccccc)); // TODO: We don't want to cast for floats
    }

	void switchEndian() noexcept { bigEndian = !bigEndian; }
	void setEndian(bool big)	noexcept { bigEndian = big; }
	bool getIsBigEndian() const noexcept { return bigEndian; }


	template <typename T, EndianSelect E>
	inline T endianDecode(T val) const noexcept
	{
#if MULTIENDIAN_SUPPORT == 0
		return val;
#else

		bool be = false;

		switch (E)
		{
		case EndianSelect::Current:
			be = bigEndian;
			break;
		case EndianSelect::Big:
			be = true;
			break;
		case EndianSelect::Little:
			be = false;
			break;
		}

		return
#if PLATFORM_LE == 0
			!
#endif
			be ?

			swapEndian<T>(val) : val;
#endif
	}

private:
	bool bigEndian; // to swap


};

} // namespace DataBlock