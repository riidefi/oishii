#pragma once

#include "memory_reader.hxx"
#include "indirection.hxx"
#include "../util/util.hxx"

#include <array>
#include <string>

namespace oishii {

// Trait
struct Invalidity
{
	enum { invality_t };
};


class BinaryReader final : public MemoryBlockReader
{
public:
	//! @brief The constructor
	//!
	//! @param[in] sz Size of the buffer to create.
	//! @param[in] f  Filename
	//!
	BinaryReader(u32 sz, const char* f = "")
		: MemoryBlockReader(sz), file(f)
	{}

	//! @brief The constructor
	//!
	//! @param[in] buf The buffer to manage.
	//! @param[in] sz  Size of the buffer.
	//! @param[in] f   Filename
	//!
	BinaryReader(std::vector<u8> buf, u32 sz, const char* f = "")
		: MemoryBlockReader(std::move(buf), sz), file(f)
	{}

	//! @brief The destructor.
	//!
	~BinaryReader() final {}

	//! @brief Given a type T, return T in the specified endiannes. (Current: swap endian if reader endian != sys endian)
	//!
	//! @tparam T Type of value to decode and return.
	//! @tparam E Endian transformation. Current will decode the value based on the endian switch flag
	//!
	//! @return T, endian decoded.
	//!
	template <typename T, EndianSelect E = EndianSelect::Current>
	inline T endianDecode(T val) const noexcept;

	template <typename T, EndianSelect E = EndianSelect::Current>
	T peek();
	template <typename T, EndianSelect E = EndianSelect::Current>
	T read();

	template <typename T, int num = 1, EndianSelect E = EndianSelect::Current>
	std::array<T, num> readX();

	template <typename T, EndianSelect E = EndianSelect::Current>
	void transfer(T& out)
	{
		out = read<T>();
	}

	template <typename T, EndianSelect E = EndianSelect::Current>
	T peekAt(int trans);
	//	template <typename T, EndianSelect E = EndianSelect::Current>
	//	T readAt();


private:
	template<typename THandler, typename Indirection, typename TContext, bool needsSeekBack = true>
	inline void invokeIndirection(TContext& ctx, u32 atPool=0);

public:
	//! @brief Dispatch an indirect data read to a handler.
	//!
	//! @tparam THandler		The handler.
	//! @tparam TIndirection	The sequence necessary to derive the value.
	//! @tparam seekback		Whether or not the reader should be restored to the end of the first indirection jump.
	//! @tparam TContext		Type of value to pass to handler.
	//!
	template <typename THandler, typename TIndirection = Direct, bool seekBack = true, typename TContext>
	void dispatch(TContext& ctx, u32 atPool=0);

	struct ScopedRegion
	{
		ScopedRegion(BinaryReader& reader, const char* name)
			: mReader(reader)
		{
			mReader.enterRegion(name, jump_save, jump_size_save, reader.tell(), 0);
		}
		~ScopedRegion()
		{
			mReader.exitRegion(jump_save, jump_size_save);
		}

	private:
		u32 jump_save;
		u32 jump_size_save;
		BinaryReader& mReader;
	};

	//! @brief Warn that there is an issue in a certain range. Stack trace is read and reported.
	//!
	//! @param[in] msg			Message to print
	//! @param[in] selectBegin	File offset where warning selection begins.
	//! @param[in] selectEnd	File offset where warning selection ends.
	//! @param[in] checkStack	Whether or not to output a stack trace. Necessary to prevent infinite recursion.
	//!
	void warnAt(const char* msg, u32 selectBegin, u32 selectEnd, bool checkStack = true);

	template<typename lastReadType, typename TInval>
	void signalInvalidityLast();

	template<typename lastReadType, typename TInval, int = TInval::inval_use_userdata_string>
	void signalInvalidityLast(const char* userData);

	// Magics are assumed to be 32 bit
	template<u32 magic, bool critical=true>
	inline void expectMagic();


	void switchEndian() noexcept { bigEndian = !bigEndian; }
	void setEndian(bool big)	noexcept { bigEndian = big; }
	bool getIsBigEndian() const noexcept { return bigEndian; }

	const char* getFile() const noexcept { return file; }
	void setFile(const char* f) noexcept { file = f; }

private:
	bool bigEndian; // to swap
	const char* file = "?";

	struct DispatchStack
	{
		struct Entry
		{
			u32 jump; // Offset in stream where jumped
			u32 jump_sz;

			const char* handlerName; // Name of handler
			u32 handlerStart; // Start address of handler

		};

		std::array<Entry, 16> mStack;
		u32 mSize = 0;

		void push_entry(u32 j, const char* name, u32 start = 0)
		{
			Entry& cur = mStack[mSize];
			++mSize;

			cur.jump = j;
			cur.handlerName = name;
			cur.handlerStart = start;
			cur.jump_sz = 1;
		}


	};

	DispatchStack mStack;
	u32 cblockstart = 0; // Start of current block, necessary for dispatch

	
	void boundsCheck(u32 size, u32 at) noexcept
	{
		// TODO: Implement
		if (Options::BOUNDS_CHECK && at + size > endpos())
		{
			// Fatal invalidity -- out of space
		}
	}
	void boundsCheck(u32 size) noexcept
	{
		boundsCheck(size, tell());
	}
	void alignmentCheck(u32 size, u32 at)
	{
		if (Options::ALIGNMENT_CHECK && at % size)
		{
			// TODO: Filter warnings in same scope, only print stack once.
			warnAt((std::string("Alignment error: ") + std::to_string(tell()) + " is not " + std::to_string(size) + " byte aligned.").c_str(), at, at + size, true);
		}
	}
	void alignmentCheck(u32 size)
	{
		alignmentCheck(size, tell());
	}
	void enterRegion(const char* name, u32& jump_save, u32& jump_size_save, u32 start, u32 size)
	{
		//_ASSERT(mStack.mSize < 16);
		mStack.push_entry(start, name, start);

		// Jump is owned by past block
		if (mStack.mSize > 1)
		{
			jump_save = mStack.mStack[mStack.mSize - 2].jump;
			jump_size_save = mStack.mStack[mStack.mSize - 2].jump_sz;
			mStack.mStack[mStack.mSize - 2].jump = start;
			mStack.mStack[mStack.mSize - 2].jump_sz = size;
		}
	}
	void exitRegion(u32 jump_save, u32 jump_size_save)
	{
		if (mStack.mSize > 1)
		{
			mStack.mStack[mStack.mSize - 2].jump = jump_save;
			mStack.mStack[mStack.mSize - 2].jump_sz = jump_size_save;
		}

		--mStack.mSize;
	}
};



#define READER_HANDLER_DECL(hname, desc, ctx_t) \
	struct hname { static constexpr char name[] = desc; \
		static inline void onRead(oishii::BinaryReader& reader, ctx_t ctx); };
#define READER_HANDLER_IMPL(hname, ctx_t) \
	void hname::onRead(oishii::BinaryReader& reader, ctx_t ctx)
#define READER_HANDLER(hname, desc, ctx_t) \
	READER_HANDLER_DECL(hname, desc, ctx_t) \
	READER_HANDLER_IMPL(hname, ctx_t)


} // namespace oishii

#include "binary_reader_impl.hxx"
#include "invalidities.hxx"
#include "stream_raii.hpp"
