#pragma once

#include "binary_reader.hxx"

namespace oishii {

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

} // namespace oishii
