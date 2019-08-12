#ifndef __STRING_SEQUENCE_H__
#define __STRING_SEQUENCE_H__

#include <sequence.h> // Seq

/**
  @brief Create a new Sequence from splitting a string on a fixed delimiter
  @param [in] str String to split.
  @param [in] delimiter The delimiter, a fixed string.
  @return A pointer to the always created Sequence
  */
Seq *SeqStringFromString(const char *str, char delimiter);

/**
 * @brief Return the total string length of a sequence of strings
 */
int SeqStringLength(Seq *seq);

/**
 * @brief Serializes a sequence of strings to a length prefixed format
 *
 * (De)Serialize uses a length prefixed format.
 * For every element in a string sequence,
 * the serialized output includes:
 * 1. 10 bytes of length prefix, where index 9 must be a space
 * 2. The data, with no escaping / modifications
 * 3. A single newline (\n) for readability
 * It is assumed that the sequence contains ascii printable
 * NUL terminated characters.
 */
char *SeqStringSerialize(Seq *seq);

/**
 * @brief Create a sequence of strings from the serialized format
 *
 * @param[in] serialized The input string, contents are copied
 * @return A sequence of new allocated strings
 */
Seq *SeqStringDeserialize(const char *const serialized);

#endif // __STRING_SEQUENCE_H__
