#ifndef __XT_RESULT_H__
#define __XT_RESULT_H__

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef signed long long int XTResult;

#define XT_TYPE_MASK(x) ((x) << 59)
#define XT_ERROR_MASK XT_TYPE_MASK(0xfull)
#define XT_WARNING_MASK XT_TYPE_MASK(0x8ull)

#define XT_ERROR(x) ((x) | XT_ERROR_MASK)
#define XT_WARNING(x) ((x) | XT_WARNING_MASK)

#define XT_IS_ERROR(x) (((x) & XT_ERROR_MASK) == XT_ERROR_MASK)
#define XT_IS_WARNING(x) (((x) & XT_WARNING_MASK) == XT_WARNING_MASK)

#define XT_SUCCESS 0
#define XT_INVALID_PARAMETER   XT_ERROR(1)
#define XT_NOT_IMPLEMENTED     XT_ERROR(2)
#define XT_OUT_OF_MEMORY       XT_ERROR(3)
#define XT_OUT_OF_BOUNDARY     XT_ERROR(4)
#define XT_NOT_FOUND           XT_ERROR(5)
#define XT_NOT_EQUAL           XT_ERROR(6)

#define XT_TRY(x) do {XTResult __result = x; if (XT_IS_ERROR(__result)) return __result;} while(0)

#define XT_CHECK_ARG_IS_NULL(x) do {if ((x) == NULL) return XT_INVALID_PARAMETER;} while(0)

#endif