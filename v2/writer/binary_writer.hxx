#pragma once

#include <vector>
#include <string>

#include "vector_writer.hxx"
#include "../../util/util.hxx"
#include "link.hxx"

namespace oishii::v2 {

class Writer : public VectorWriter
{
public:
	Writer(u32 size)
		: VectorWriter(size)
	{}
	Writer(std::vector<u8> buf)
		: VectorWriter(std::move(buf))
	{}

	template<typename T, EndianSelect E = EndianSelect::Current>
	void transfer(T& out)
	{
		write<T>(out);
	}

	template <typename T, EndianSelect E = EndianSelect::Current>
	void write(T val, bool checkmatch=true)
	{
		while (tell() + sizeof(T) > mBuf.size())
			mBuf.push_back(0);

		breakPointProcess(sizeof(T));

		const auto decoded = endianDecode<T, E>(val);

#ifndef NDEBUG
		if (checkmatch && mDebugMatch.size() > tell() && !std::is_floating_point_v<T>)
		{
			const auto before = *reinterpret_cast<T*>(&mDebugMatch[tell()]);
			if (before != decoded && decoded != (T)0xcccccccc)
			{
				printf("Matching violation at %x: writing %x where should be %x\n", tell(), decoded, before);
				__debugbreak();
			}
		}
#endif

		*reinterpret_cast<T*>(&mBuf[tell()]) = decoded;

		seek<Whence::Current>(sizeof(T));
	}

	template <EndianSelect E = EndianSelect::Current>
	void writeN(std::size_t sz, u32 val)
	{
		while (tell() + sz > mBuf.size())
			mBuf.push_back(0);

		u32 decoded = endianDecode<u32, E>(val);

#if 0
#ifndef NDEBUG1
		if (/*checkmatch && */mDebugMatch.size() > tell() + sz)
		{
			for (int i = 0; i < sz; ++i)
			{
				const auto before = mDebugMatch[tell() + i];
				if (before != decoded >> (8 * i))
				{
					printf("Matching violation at %x: writing %x where should be %x\n", tell(), decoded, before);
					__debugbreak();
				}
			}
		}
#endif
#endif
		for (int i = 0; i < sz; ++i)
			mBuf[tell() + i] = static_cast<u8>(decoded >> (8 * i));

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

	template<typename T>
	void writeLink(const Hook& from, const Hook& to)
	{
		writeLink<T>(Link{ from, to });
	}

	void switchEndian() noexcept { bigEndian = !bigEndian; }
	void setEndian(bool big)	noexcept { bigEndian = big; }
	bool getIsBigEndian() const noexcept { return bigEndian; }


	template <typename T, EndianSelect E>
	inline T endianDecode(T val) const noexcept
	{
		if (!Options::MULTIENDIAN_SUPPORT)
			return val;

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

		if (!Options::PLATFORM_LE)
			be = !be;

		return be ? swapEndian<T>(val) : val;
	}

private:
	bool bigEndian = true; // to swap
};

}
