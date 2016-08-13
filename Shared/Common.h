/**
 * @file Common.h
 * @author biko
 * @date 2016-07-30
 *
 * Common definitions for the solution.
 */
#pragma once

/** Macros **************************************************************/

/**
 * Closes an object using a destructor function,
 * then resets the object to the given invalid value.
 * Optionally passes additional arguments to the destructor.
 */
#define CLOSE_TO_VALUE_VARIADIC(object, pfnDestructor, value, ...)	\
	do																\
	{																\
		if ((value) != (object))									\
		{															\
			(VOID)(pfnDestructor)((object), __VA_ARGS__);			\
			(object) = (value);										\
		}															\
	} while (0)

/**
 * Closes an object using a destructor function,
 * then resets the object to the given invalid value.
 */
#define CLOSE_TO_VALUE(object, pfnDestructor, value) \
	CLOSE_TO_VALUE_VARIADIC((object), (pfnDestructor), (value))

/**
 * Closes an object using a destructor function,
 * then resets it to NULL.
 */
#define CLOSE(object, pfnDestructor) CLOSE_TO_VALUE((object), (pfnDestructor), NULL)
