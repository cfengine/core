#include <platform.h>
#include <validate.h>
#include <logging.h>

#if defined(__MINGW32__) || !defined(LMDB)

int CFCheck_Validate(const char *path)
{
    Log(LOG_LEVEL_ERR,
        "cf-check diagnose --validate not available on this platform/build");
    // Cannot include utilities.h on Windows:
    return -1; // CF_CHECK_ERRNO_VALIDATE_FAILED
}

#else

#include <lmdb.h>       // mdb_open(), mdb_close(), mdb_strerror()
#include <alloc.h>      // xcalloc(), xmemdup(), xstrdup()
#include <string_lib.h> // StringEndsWith()
#include <map.h>        // StringMap
#include <set.h>        // StringSet
#include <diagnose.h>   // CF_CHECK_ERRNO_VALIDATE_FAILED
#include <db_structs.h> // KeyHostSeen

#define CF_BIRTH 725846400 // Assume timestamps before 1993-01-01 are corrupt

typedef enum ValidatorMode
{
    CF_CHECK_VALIDATE_UNKNOWN,
    CF_CHECK_VALIDATE_MINIMAL,
    CF_CHECK_VALIDATE_LASTSEEN,
} ValidatorMode;

typedef struct LastSeenState
{
    StringMap *hostkey_to_address;
    StringMap *address_to_hostkey;
    StringSet *quality_outgoing_hostkeys;
    StringSet *quality_incoming_hostkeys;
} LastSeenState;

typedef struct ValidatorState
{
    const char *path;
    ValidatorMode mode;
    size_t errors;
    StringSet *keys;
    Seq *values;
    union
    {
        LastSeenState lastseen;
    };
} ValidatorState;

static Slice *NewLMDBSlice(void *data, size_t size)
{
    assert(data != NULL);
    assert(size > 0);

    Slice *r = xcalloc(1, sizeof(Slice));
    r->size = size;
    r->data = xmemdup(data, size);

    return r;
}

static void DestroyLMDBSlice(Slice *value)
{
    if (value != NULL)
    {
        free(value->data);
        free(value);
    }
}

static void NewValidator(const char *path, ValidatorState *state)
{
    assert(state != NULL);

    state->path = path;
    state->errors = 0;

    state->keys = StringSetNew();
    state->values = SeqNew(0, &DestroyLMDBSlice);

    if (StringEndsWith(path, "cf_lastseen.lmdb"))
    {
        state->mode = CF_CHECK_VALIDATE_LASTSEEN;
        state->lastseen.hostkey_to_address = StringMapNew();
        state->lastseen.address_to_hostkey = StringMapNew();
        state->lastseen.quality_outgoing_hostkeys = StringSetNew();
        state->lastseen.quality_incoming_hostkeys = StringSetNew();
    }
    else if (StringEndsWith(path, "cf_changes.lmdb"))
    {
        state->mode = CF_CHECK_VALIDATE_MINIMAL;
    }
    else
    {
        state->mode = CF_CHECK_VALIDATE_UNKNOWN;
    }
}

static void DestroyValidator(ValidatorState *state)
{
    // Since state is expected to be stack allocated, we don't allow NULL
    // pointer. (We normally do for heap allocated data structures).
    assert(state != NULL);

    state->path = NULL;

    StringSetDestroy(state->keys);
    SeqDestroy(state->values);

    switch (state->mode)
    {
    case CF_CHECK_VALIDATE_LASTSEEN:
        StringMapDestroy(state->lastseen.hostkey_to_address);
        StringMapDestroy(state->lastseen.address_to_hostkey);
        StringSetDestroy(state->lastseen.quality_outgoing_hostkeys);
        StringSetDestroy(state->lastseen.quality_incoming_hostkeys);
        break;
    case CF_CHECK_VALIDATE_MINIMAL:
    case CF_CHECK_VALIDATE_UNKNOWN:
        break;
    default:
        debug_abort_if_reached();
        break;
    }
}

static void va_ValidationError(
    ValidatorState *state, const char *fmt, va_list ap)
{
    assert(state != NULL && state->path != NULL);
    assert(fmt != NULL);

    printf("Error in %s: ", state->path);
    vprintf(fmt, ap);
    printf("\n");
    state->errors += 1;
}

static void ValidationError(ValidatorState *state, const char *fmt, ...)
    FUNC_ATTR_PRINTF(2, 3);

static void ValidationError(ValidatorState *state, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_ValidationError(state, fmt, ap);
    va_end(ap);
}

static bool ValidateString(ValidatorState *state, MDB_val string)
{
    assert(state != NULL);

    const char *const str = string.mv_data;

    if (strnlen(str, string.mv_size) != string.mv_size - 1)
    {
        ValidationError(state, "Invalid string - '%s'", str);
        return false;
    }

    if (string.mv_size == 1)
    {
        ValidationError(state, "Invalid string - empty");
        return false;
    }

    return true;
}

// Should be used after validating that hostkey is a NUL-terminated string
static bool ValidateHostkey(ValidatorState *state, const char *hostkey)
{
    if (EmptyString(hostkey))
    {
        // For example a key of "k", missing the "kSHA=..." parts
        ValidationError(state, "Empty hostkey - '%s'", hostkey);
        return false;
    }

    // Example hostkeys:
    // MD5=14f11b956431e401ba60861402be3b9c
    // SHA=e7f1fd5ea18f9d593641f4168d6140ab362f6c02cb1275e41faa17716db457ea

    if (StringStartsWith(hostkey, "SHA="))
    {
        if (strlen(hostkey + 4) != 64)
        {
            ValidationError(state, "Bad length for hostkey - '%s'", hostkey);
            return false;
        }
    }
    else if (StringStartsWith(hostkey, "MD5="))
    {
        if (strlen(hostkey + 4) != 32)
        {
            ValidationError(state, "Bad length for hostkey - '%s'", hostkey);
            return false;
        }
    }
    else
    {
        ValidationError(state, "Unknown format of hostkey - '%s'", hostkey);
        return false;
    }

    return true;
}

static bool ValidateAddress(ValidatorState *state, const char *address)
{
    if (EmptyString(address))
    {
        // For example a key of "a", missing the "a1.2.3.4" parts
        ValidationError(state, "Empty IP address - '%s'", address);
        return false;
    }

    return true;
}

static void UpdateValidatorLastseen(
    ValidatorState *state, MDB_val key, MDB_val value)
{
    assert(state != NULL);
    assert(key.mv_size > 0 && key.mv_data != NULL);
    assert(value.mv_size > 0 && value.mv_data != NULL);

    const LastSeenState ls = state->lastseen;
    StringMap *const hostkey_to_address = ls.hostkey_to_address;
    StringMap *const address_to_hostkey = ls.address_to_hostkey;
    StringSet *const quality_outgoing_hostkeys = ls.quality_outgoing_hostkeys;
    StringSet *const quality_incoming_hostkeys = ls.quality_incoming_hostkeys;

    const char *key_string = key.mv_data;

    if (StringStartsWith(key_string, "k"))
    {
        if (!ValidateString(state, value))
        {
            return;
        }

        char *hostkey = xstrdup(key_string + 1);
        char *address = xstrdup(value.mv_data);

        // This is an assert, because:
        // * Only this branch adds to this data structure
        // * UpdateValidator() has already checked for duplicate keys
        // (Same applies to the other branches below.)
        assert(StringMapGet(hostkey_to_address, hostkey) == NULL);

        StringMapInsert(hostkey_to_address, hostkey, address);
    }
    else if (StringStartsWith(key_string, "a"))
    {
        if (!ValidateString(state, value))
        {
            return;
        }

        char *address = xstrdup(key_string + 1);
        char *hostkey = xstrdup(value.mv_data);
        assert(StringMapGet(address_to_hostkey, address) == NULL);
        StringMapInsert(address_to_hostkey, address, hostkey);
    }
    else if (StringStartsWith(key_string, "qo"))
    {
        const char *hostkey = key_string + 2;
        assert(!StringSetContains(quality_outgoing_hostkeys, hostkey));
        StringSetAdd(quality_outgoing_hostkeys, xstrdup(hostkey));
    }
    else if (StringStartsWith(key_string, "qi"))
    {
        const char *hostkey = key_string + 2;
        assert(!StringSetContains(quality_incoming_hostkeys, hostkey));
        StringSetAdd(quality_incoming_hostkeys, xstrdup(hostkey));
    }

    if (key_string[0] == 'q')
    {
        const char direction = key_string[1];
        if (direction == 'i' || direction == 'o')
        {
            const KeyHostSeen *const data = value.mv_data;

            const time_t lastseen = data->lastseen;
            const time_t current = time(NULL);

            Log(LOG_LEVEL_DEBUG,
                "LMDB validation: Quality-entry lastseen time is %ju, current time is %ju",
                (uintmax_t) lastseen,
                (uintmax_t) current);

            if (current < CF_BIRTH)
            {
                ValidationError(
                    state,
                    "Current time (%ju) is before 1993-01-01",
                    (uintmax_t) current);
            }
            else if (lastseen < CF_BIRTH)
            {
                ValidationError(
                    state,
                    "Last seen time (%ju) is before 1993-01-01 (%s)",
                    (uintmax_t) lastseen,
                    key_string);
            }
            else if (lastseen > current)
            {
                ValidationError(
                    state,
                    "Future timestamp in last seen database: %ju > %ju (%s)",
                    (uintmax_t) lastseen,
                    (uintmax_t) current,
                    key_string);
            }
        }
        else
        {
            ValidationError(
                state, "Unexpected quality-entry key: %s", key_string);
        }
    }
}

static bool ValidateMDBValue(
    ValidatorState *state, MDB_val value, const char *name)
{
    if (value.mv_size <= 0)
    {
        ValidationError(state, "0 size %s", name);
        return false;
    }
    if (value.mv_data == NULL)
    {
        ValidationError(state, "NULL %s", name);
        return false;
    }
    return true;
}

static void UpdateValidator(ValidatorState *state, MDB_val key, MDB_val value)
{
    assert(state != NULL);

    if (state->mode == CF_CHECK_VALIDATE_MINIMAL)
    {
        // Databases with "weird" schemas, i.e. non-string keys,
        // just check that we can read out the data:
        void *key_copy = xmemdup(key.mv_data, key.mv_size);
        void *value_copy = xmemdup(value.mv_data, value.mv_size);
        free(key_copy);
        free(value_copy);
        return;
    }

    if (!ValidateMDBValue(state, key, "key") || !ValidateString(state, key)
        || !ValidateMDBValue(state, value, "value"))
    {
        return;
    }

    const char *const key_string = key.mv_data;

    if (StringSetContains(state->keys, key_string))
    {
        ValidationError(state, "Duplicate key - '%s'", key_string);
        return;
    }

    Log(LOG_LEVEL_DEBUG,
        "LMDB validation: Adding key '%s'",
        (const char *) key.mv_data);

    StringSetAdd(state->keys, xstrdup(key.mv_data));

    SeqAppend(state->values, NewLMDBSlice(value.mv_data, value.mv_size));

    switch (state->mode)
    {
    case CF_CHECK_VALIDATE_LASTSEEN:
        UpdateValidatorLastseen(state, key, value);
        break;
    case CF_CHECK_VALIDATE_UNKNOWN:
        break;
    default:
        debug_abort_if_reached();
        break;
    }

    return;
}

static void ValidateStateLastseen(ValidatorState *state)
{
    const LastSeenState ls = state->lastseen;
    StringMap *const hostkey_to_address = ls.hostkey_to_address;
    StringMap *const address_to_hostkey = ls.address_to_hostkey;
    StringSet *const quality_outgoing_hostkeys = ls.quality_outgoing_hostkeys;
    StringSet *const quality_incoming_hostkeys = ls.quality_incoming_hostkeys;

    assert(state != NULL);

    {
        MapIterator iter = MapIteratorInit(hostkey_to_address->impl);
        MapKeyValue *current_item;
        while ((current_item = MapIteratorNext(&iter)) != NULL)
        {
            const char *hostkey = current_item->key;
            if (!ValidateHostkey(state, hostkey))
            {
                continue;
            }
            const char *address = current_item->value;
            if (!ValidateAddress(state, address))
            {
                continue;
            }
            const char *lookup = StringMapGet(address_to_hostkey, address);
            if (lookup == NULL)
            {
                ValidationError(
                    state, "Missing address entry for '%s'", address);
            }
            else if (!StringSafeEqual(hostkey, lookup))
            {
                ValidationError(
                    state,
                    "Bad hostkey->address->hostkey reverse lookup '%s' != '%s'",
                    hostkey,
                    lookup);
            }
        }
    }
    {
        MapIterator iter = MapIteratorInit(address_to_hostkey->impl);
        MapKeyValue *current_item;
        while ((current_item = MapIteratorNext(&iter)) != NULL)
        {
            const char *address = current_item->key;
            if (!ValidateAddress(state, address))
            {
                continue;
            }
            const char *hostkey = current_item->value;
            if (!ValidateHostkey(state, hostkey))
            {
                continue;
            }
            const char *lookup = StringMapGet(hostkey_to_address, hostkey);
            if (lookup == NULL)
            {
                ValidationError(
                    state, "Missing hostkey entry for '%s'", hostkey);
            }
            else if (!StringSafeEqual(address, lookup))
            {
                ValidationError(
                    state,
                    "Bad address->hostkey->address reverse lookup '%s' != '%s'",
                    address,
                    lookup);
            }
        }
    }
    {
        StringSetIterator iter =
            StringSetIteratorInit(quality_incoming_hostkeys);
        const char *hostkey;
        while ((hostkey = StringSetIteratorNext(&iter)) != NULL)
        {
            if (StringMapGet(hostkey_to_address, hostkey) == NULL)
            {
                ValidationError(
                    state,
                    "Missing hostkey from quality-in entry '%s'",
                    hostkey);
            }
        }
    }
    {
        StringSetIterator iter =
            StringSetIteratorInit(quality_outgoing_hostkeys);
        const char *hostkey;
        while ((hostkey = StringSetIteratorNext(&iter)) != NULL)
        {
            if (StringMapGet(hostkey_to_address, hostkey) == NULL)
            {
                ValidationError(
                    state,
                    "Missing hostkey from quality-out entry '%s'",
                    hostkey);
            }
        }
    }
}

static void ValidateState(ValidatorState *state)
{
    assert(state != NULL);

    switch (state->mode)
    {
    case CF_CHECK_VALIDATE_LASTSEEN:
        ValidateStateLastseen(state);
    case CF_CHECK_VALIDATE_UNKNOWN:
    case CF_CHECK_VALIDATE_MINIMAL:
        break;
    default:
        debug_abort_if_reached();
        break;
    }
}

int CFCheck_Validate(const char *path)
{
    assert(path != NULL);

    MDB_env *env;
    int rc = mdb_env_create(&env);
    if (rc != 0)
    {
        return rc;
    }

    rc = mdb_env_open(env, path, MDB_NOSUBDIR | MDB_RDONLY, 0644);
    if (rc != 0)
    {
        return rc;
    }

    MDB_txn *txn;
    rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
    if (rc != 0)
    {
        return rc;
    }

    MDB_dbi dbi;
    rc = mdb_open(txn, NULL, 0, &dbi);
    if (rc != 0)
    {
        return rc;
    }

    MDB_cursor *cursor;
    rc = mdb_cursor_open(txn, dbi, &cursor);
    if (rc != 0)
    {
        return rc;
    }

    ValidatorState state;
    NewValidator(path, &state);
    MDB_val key, data;
    while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == MDB_SUCCESS)
    {
        UpdateValidator(&state, key, data);
    }
    if (rc != MDB_NOTFOUND)
    {
        // At this point, not found is expected, anything else is an error
        DestroyValidator(&state);
        return rc;
    }
    mdb_cursor_close(cursor);
    mdb_close(env, dbi);

    mdb_txn_abort(txn);
    mdb_env_close(env);

    ValidateState(&state);
    const size_t errors = state.errors;
    DestroyValidator(&state);

    // TODO: Find a better return code.
    //       This is mapped to errno, so 1 is definitely wrong
    return (errors == 0) ? 0 : CF_CHECK_ERRNO_VALIDATE_FAILED;
}

#endif
