#pragma once

#include "log.h"
#include "string_ref.h"

#include <string.h>

/** Checks `handle`. Catches exceptions and puts them into the DiagnosticRecord.
  */
template <typename Handle, typename F>
RETCODE doWith(SQLHANDLE handle_opaque, F && f)
{
    if (nullptr == handle_opaque)
        return SQL_INVALID_HANDLE;

    Handle & handle = *reinterpret_cast<Handle *>(handle_opaque);

    try
    {
        handle.diagnostic_record.reset();
        return f(handle);
    }
    catch (...)
    {
        handle.diagnostic_record.fromException();
        return SQL_ERROR;
    }
}


/// Parse a string of the form `key1=value1;key2=value2` ... TODO Parsing values in curly brackets.
static LPCTSTR nextKeyValuePair(LPCTSTR data, LPCTSTR end, StringRef & out_key, StringRef & out_value)
{
    if (data >= end)
        return nullptr;

    LPCTSTR key_begin = data;
    LPCTSTR key_end = reinterpret_cast<LPCTSTR>(memchr(key_begin, '=', end - key_begin));
    if (!key_end)
        return nullptr;

    LPCTSTR value_begin = key_end + 1;
    LPCTSTR value_end;
    if (value_begin >= end)
        value_end = value_begin;
    else
    {
        value_end = reinterpret_cast<LPCTSTR>(memchr(value_begin, ';', end - value_begin));
        if (!value_end)
            value_end = end;
    }

    out_key.data = key_begin;
    out_key.size = key_end - key_begin;

    out_value.data = value_begin;
    out_value.size = value_end - value_begin;

    if (value_end < end && *value_end == ';')
        return value_end + 1;
    return value_end;
}


template <typename SIZE_TYPE>
std::string stringFromSQLChar(SQLTCHAR * data, SIZE_TYPE size)
{
    if (!data)
        return {};

    if (size < 0)
        size = (SIZE_TYPE)strlen(reinterpret_cast<LPCTSTR>(data));

    return { reinterpret_cast<LPCTSTR>(data), static_cast<size_t>(size) };
}


template <typename PTR, typename LENGTH>
RETCODE fillOutputString(LPCTSTR value, size_t size_without_zero,
    PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length)
{
    if (out_value_length)
        *out_value_length = (LENGTH)size_without_zero;

    if (out_value_max_length < 0)
        return SQL_ERROR;

    bool res = SQL_SUCCESS;

    if (out_value)
    {
        if (out_value_max_length >= static_cast<LENGTH>(size_without_zero + 1))
        {
            memcpy(out_value, value, (size_without_zero + 1) * sizeof(TCHAR));
        }
        else
        {
            if (out_value_max_length > 0)
            {
                memcpy(out_value, value, (out_value_max_length - 1) * sizeof(TCHAR));
                reinterpret_cast<LPTSTR>(out_value)[out_value_max_length - 1] = 0;

                LOG((LPCTSTR)(out_value));
            }
            res = SQL_SUCCESS_WITH_INFO;
        }
    }

    return res;
}

template <typename PTR, typename LENGTH>
RETCODE fillOutputString(LPCTSTR value,
    PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length)
{
    return fillOutputString(value, strlen(value), out_value, out_value_max_length, out_value_length);
}

template <typename PTR, typename LENGTH>
RETCODE fillOutputString(const std::string & value,
    PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length)
{
    return fillOutputString(value.data(), value.size(), out_value, out_value_max_length, out_value_length);
}


template <typename NUM, typename PTR, typename LENGTH>
RETCODE fillOutputNumber(NUM num,
    PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length)
{
    if (out_value_length)
        *out_value_length = sizeof(num);

    if (out_value_max_length < 0)
        return SQL_ERROR;

    bool res = SQL_SUCCESS;

    if (out_value)
    {
        if (out_value_max_length == 0 || out_value_max_length >= static_cast<LENGTH>(sizeof(num)))
        {
            memcpy(out_value, &num, sizeof(num));
        }
        else
        {
            memcpy(out_value, &num, out_value_max_length);
            res = SQL_SUCCESS_WITH_INFO;
        }
    }

    return res;
}


/// See for example info.cpp

#define CASE_FALLTHROUGH(NAME) \
    case NAME: \
        if (!name) name = #NAME;

#define CASE_STRING(NAME, VALUE) \
    case NAME: \
        if (!name) name = #NAME; \
        LOG("GetInfo " << name << ", type: String, value: " << (VALUE)); \
        return fillOutputString(VALUE, out_value, out_value_max_length, out_value_length);

#define CASE_NUM(NAME, TYPE, VALUE) \
    case NAME: \
        if (!name) name = #NAME; \
        LOG("GetInfo " << name << ", type: " << #TYPE << ", value: " << #VALUE << " = " << (VALUE)); \
        return fillOutputNumber<TYPE>(VALUE, out_value, out_value_max_length, out_value_length);
