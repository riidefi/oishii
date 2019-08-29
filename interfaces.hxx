#pragma once

#include "types.hxx"

namespace oishii {

enum class Whence
{
	Set, // Absolute -- start of file
	Current, // Relative -- current position
	End, // Relative -- end position
	// Only valid for invalidities so far
	Last, // Relative -- last read value

	At, // Indirection -- use specified translation (runtime) argument
};

class AbstractStream
{
public:
	virtual u32 tell() = 0;
	virtual void seekSet(u32 ofs) = 0;
	virtual u32 startpos() = 0;
	virtual u32 endpos() = 0;
};

class IReader : public AbstractStream
{
public:

	virtual char* getStreamStart() = 0;
};

class IWriter : public AbstractStream
{
};
} // namespace oishii