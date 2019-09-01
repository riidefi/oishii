#pragma once

#include "memory_reader.hxx"
#include "indirection.hxx"
#include "../util/util.hxx"

#include <array>

namespace oishii {

// Trait
struct Invalidity
{
	enum { invality_t };
};


template <u32 m>
struct MagicInvalidity;


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
	BinaryReader(std::unique_ptr<char> buf, u32 sz, const char* f = "")
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
	template<u32 magic>
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
};

template<Whence W = Whence::Current, typename T = BinaryReader>
struct Jump
{
	inline Jump(T& stream, u32 offset)
		: mStream(stream), back(stream.tell())
	{
		mStream.seek<W>(offset);
	}
	inline ~Jump()
	{
		mStream.seek<Whence::Set>(back);
	}
	T& mStream;
	u32 back;
};

template<Whence W = Whence::Current, typename T = BinaryReader>
struct JumpOut
{
	inline JumpOut(T& stream, u32 offset)
		: mStream(stream), start(stream.tell()), back(offset)
	{}
	inline ~JumpOut()
	{
		mStream.seek<Whence::Set>(start);
		mStream.seek<W>(back);
	}
	T& mStream;
	u32 back;
	u32 start;
};

template<typename T = BinaryReader>
struct DebugExpectSized
#ifdef RELEASE
{
	DebugExpectSized(T& stream, u32 size) {}
};
#else
{
	inline DebugExpectSized(T& stream, u32 size)
		: mStream(stream), mStart(stream.tell()), mSize(size)
	{}
	inline ~DebugExpectSized()
	{
		assert(mStream.tell() - mStart == mSize && "Invalid size for this scope!");
	}

	T& mStream;
	u32 mSize;
	u32 mStart;
};
#endif
template <typename T, EndianSelect E>
inline T BinaryReader::endianDecode(T val) const noexcept
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

	if (Options::PLATFORM_LE)
		be = !be;

	return be ? swapEndian<T>(val) : val;

}

template <typename T, EndianSelect E>
T BinaryReader::peek()
{
#if DO_BOUNDS_CHECK == 1
	if (tell() + sizeof(T) > endpos())
	{
		// Fatal invalidity -- out of space
		//throw "FATAL: TODO";

		return T{};
	}
#endif
	T decoded = endianDecode<T, E>(*reinterpret_cast<T*>(getStreamStart() + tell()));
	
	return decoded;
}
template <typename T, EndianSelect E>
T BinaryReader::read()
{
	T decoded = peek<T, E>();
	seek<Whence::Current>(sizeof(T));
	return decoded;
}
// TODO: Can rewrite read/peek to use peekAt
template <typename T, EndianSelect E>
T BinaryReader::peekAt(int trans)
{
	if (Options::DO_BOUNDS_CHECK && tell() + sizeof(T) > endpos())
	{
		// Fatal invalidity -- out of space
		//throw "FATAL: TODO";

		return T{};
	}

	T decoded = endianDecode<T, E>(*reinterpret_cast<T*>(getStreamStart() + tell() + trans));

	return decoded;
}
//template <typename T, EndianSelect E>
//T BinaryReader::readAt(int trans)
//{
//	T decoded = peekAt<T, E>(trans);
//	return decoded;
//}

template<typename THandler, typename Indirection, typename TContext, bool needsSeekBack>
inline void BinaryReader::invokeIndirection(TContext& ctx, u32 atPool)
{
	const u32 start = tell();
	const u32 ptr = Indirection::is_pointed ? (u32)read<typename Indirection::offset_t>() : 0;
	const u32 back = tell();

	seek<Indirection::whence>(ptr + Indirection::translation, atPool);
	if (Indirection::has_next)
	{
		invokeIndirection<THandler, typename Indirection::next, TContext, false>(ctx);
	}
	else
	{
		//_ASSERT(mStack.mSize < 16);
		mStack.push_entry(tell(), THandler::name, tell());


		u32 jump_save = 0;
		u32 jump_size_save = 0;
		// Jump is owned by past block
		if (mStack.mSize > 1)
		{
			jump_save = mStack.mStack[mStack.mSize - 2].jump;
			jump_size_save = mStack.mStack[mStack.mSize - 2].jump_sz;
			mStack.mStack[mStack.mSize - 2].jump = start;
			mStack.mStack[mStack.mSize - 2].jump_sz = sizeof(typename Indirection::offset_t); // TODO: For direct, is it really showing 0?
		}
		cblockstart = tell();
		THandler::onRead(*this, ctx);

		if (mStack.mSize > 1)
		{
			mStack.mStack[mStack.mSize - 2].jump = jump_save;
			mStack.mStack[mStack.mSize - 2].jump_sz = jump_size_save;
		}

		--mStack.mSize;
	}
	// With multiple levels of indirection, we only need to seek back on the outermost layer
	if (needsSeekBack)
		seek<Whence::Set>(back);
}

template <typename THandler,
	typename TIndirection,
	bool seekBack,
	typename TContext>
	void BinaryReader::dispatch(TContext& ctx, u32 atPool)
{
	invokeIndirection<THandler, TIndirection, TContext, seekBack>(ctx, atPool);
}
template<typename lastReadType, typename TInval>
void BinaryReader::signalInvalidityLast()
{
	// Unfortunate workaround, would prefer to do warn<type>
	TInval::warn(*this, sizeof(lastReadType));
}
template<typename lastReadType, typename TInval, int>
void BinaryReader::signalInvalidityLast(const char* msg)
{
	TInval::warn(*this, sizeof(lastReadType), msg);
}

template<u32 magic>
inline void BinaryReader::expectMagic()
{
if (read<u32, EndianSelect::Big>() != magic)
	signalInvalidityLast<u32, MagicInvalidity<magic>>();
}
template<u32 m>
struct MagicInvalidity : public Invalidity
{
	enum
	{
		c0 = (m & 0xff000000) >> 24,
		c1 = (m & 0x00ff0000) >> 16,
		c2 = (m & 0x0000ff00) >> 8,
		c3 = (m & 0x000000ff),

		magic_big = MAKE_BE32(m)
	};

	static void warn(BinaryReader& reader, u32 sz)
	{
		char tmp[] = "File identification magic mismatch: expecting XXXX, instead saw YYYY.";
		*(u32*)&tmp[46] = magic_big;
		*(u32*)&tmp[64] = *(u32*)(reader.getStreamStart() + reader.tell() - 4);
		reader.warnAt(tmp, reader.tell() - sz, reader.tell());
	}
};
struct BOMInvalidity : public Invalidity
{
	static void warn(BinaryReader& reader, u32 sz)
	{
		char tmp[] = "File bye-order-marker (BOM) is invalid. Expected either 0xFFFE or 0xFEFF.";
		reader.warnAt(tmp, reader.tell() - sz, reader.tell());
	}
};

struct UncommonInvalidity : public Invalidity
{
	enum { inval_use_userdata_string };
	static void warn(BinaryReader& reader, u32 sz, const char* msg)
	{
		reader.warnAt(msg, reader.tell() - sz, reader.tell());
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
