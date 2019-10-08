#pragma once

#include "binary_reader.hxx"

namespace oishii {

template <u32 m>
struct MagicInvalidity;

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

	if (!Options::PLATFORM_LE)
		be = !be;

	return be ? swapEndian<T>(val) : val;

}

template <typename T, EndianSelect E>
T BinaryReader::peek()
{
	boundsCheck(sizeof(T));
	alignmentCheck(sizeof(T));

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

template <typename T, int num, EndianSelect E>
std::array<T, num> BinaryReader::readX()
{
	std::array<T, num> result;

	for (auto& e : result)
		e = read<T>();

	return result;
}

// TODO: Can rewrite read/peek to use peekAt
template <typename T, EndianSelect E>
T BinaryReader::peekAt(int trans)
{
	boundsCheck(sizeof(T));

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

template<u32 magic, bool critical>
inline void BinaryReader::expectMagic()
{
	if (read<u32, EndianSelect::Big>() != magic)
	{
		signalInvalidityLast<u32, MagicInvalidity<magic>>();

		if (critical)
			exit(1);
	}
}

} // namespace oishii
