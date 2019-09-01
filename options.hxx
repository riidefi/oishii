#pragma once

namespace oishii {

#ifdef _DEBUG
	#define DEBUG
#endif

#ifdef _NDEBUG
	#define RELEASE
	#undef DEBUG
#endif


// Prefer options enumeration to macros
enum Options
{
#ifdef DEBUG
	IF_DEBUG = 1,
	#define OISHII_IF_DEBUG 1
#else
	IF_DEBUG = 0,
	#define OISHII_IF_DEBUG = 0
#endif

#ifdef OISHII_MULTIENDIAN_SUPPORT
	MULTIENDIAN_SUPPORT = OISHII_MULTIENDIAN_SUPPORT,
#else
	MULTIENDIAN_SUPPORT = 1,
	#define OISHII_MULTIENDIAN_SUPPORT 1
#endif

#ifdef OISHII_PLATFORM_LE
	PLATFORM_LE = OISHII_PLATFORM_LE,
#else
	PLATFORM_LE = 1,
	#define OISHII_PLATFORM_LE 1
#endif

	DO_BOUNDS_CHECK = 0, // TODO

#ifdef OISHII_ALIGNMENT_CHECK
	ALIGNMENT_CHECK = OISHII_ALIGNMENT_CHECK,
#else
	ALIGNMENT_CHECK = IF_DEBUG
	#define OISHII_ALIGNMENT_CHECK OISHII_IF_DEBUG
#endif
};

} // namespace oishii
