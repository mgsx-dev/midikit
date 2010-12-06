#include <stdlib.h>
#include <string.h>
#include "message_format.h"

/**
 * @struct MIDIMessageData message_format.h
 * @brief Store any kind of MIDI message.
 * Usually the message data only makes sense in combination with a message format.
 * However, there is one important thing to remember. You may only free the data
 * field if bytes[3] is one! (And you need to set it to one if you allocate some
 * buffer for it.
 *
 * The size and data fields are only used for system exclusive messages. Those
 * messages store the system exclusive data inside the data field. Status,
 * manufacturer ID and fragment number are stored in the bytes array.
 * @see MIDIMessageFormat
 */
 
/**
 * @struct MIDIMessageFormat message_format.h
 * Message format descriptor.
 * Contains pointers to functions that access messages of a certain type.
 * @see MIDIMessageData
 */
struct MIDIMessageFormat {
  int (*test)( void * buffer );
  int (*size)( struct MIDIMessageData * data, size_t * size );
  int (*set)( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value );
  int (*get)( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value );
  int (*encode)( struct MIDIMessageData * data, size_t size, void * buffer );
  int (*decode)( struct MIDIMessageData * data, size_t size, void * buffer );
};

#define VOID_BYTE( buffer, n ) ((uint8_t*)buffer)[n]

#pragma mark Encoding & decoding
/**
 * @internal
 * Encoding & decoding functions.
 * These functions encode a MIDI message to or from a stream.
 * @{
 */

static int _encode_one_byte( struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( data == NULL || buffer == NULL ) return 1;
  if( size < 1 ) return 1;
  VOID_BYTE(buffer,0) = data->bytes[0];
  return 0;
}

static int _decode_one_byte( struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( data == NULL || buffer == NULL ) return 1;
  if( size < 1 ) return 1;
  data->bytes[0] = VOID_BYTE(buffer,0);
  return 0;
}

static int _encode_two_bytes( struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( data == NULL || buffer == NULL ) return 1;
  if( size < 2 ) return 1;
  VOID_BYTE(buffer,0) = data->bytes[0];
  VOID_BYTE(buffer,1) = data->bytes[1];
  return 0;
}

static int _decode_two_bytes( struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( data == NULL || buffer == NULL ) return 1;
  if( size < 2 ) return 1;
  data->bytes[0] = VOID_BYTE(buffer,0);
  data->bytes[1] = VOID_BYTE(buffer,1);
  return 0;
}

static int _encode_three_bytes( struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( data == NULL || buffer == NULL ) return 1;
  if( size < 3 ) return 1;
  VOID_BYTE(buffer,0) = data->bytes[0];
  VOID_BYTE(buffer,1) = data->bytes[1];
  VOID_BYTE(buffer,2) = data->bytes[2];
  return 0;
}

static int _decode_three_bytes( struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( data == NULL || buffer == NULL ) return 1;
  if( size < 3 ) return 1;
  data->bytes[0] = VOID_BYTE(buffer,0);
  data->bytes[1] = VOID_BYTE(buffer,1);
  data->bytes[2] = VOID_BYTE(buffer,2);
  return 0;
}

static int _encode_system_exclusive( struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( data == NULL || buffer == NULL ) return 1;
  if( data->bytes[2] == 0 ) {
    if( size < data->size+2 ) return 1;
    VOID_BYTE(buffer,0) = data->bytes[0];
    VOID_BYTE(buffer,1) = data->bytes[1];
    if( data->size > 0 && data->data != NULL )
      memcpy( buffer+2, data->data, data->size );
  } else {
    if( size < data->size ) return 1;
    if( data->size > 0 && data->data != NULL )
      memcpy( buffer, data->data, data->size );
  }
  return 0;  
}

static int _decode_system_exclusive( struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( data == NULL || buffer == NULL ) return 1;
  data->bytes[0] = VOID_BYTE(buffer,0);
  data->bytes[1] = VOID_BYTE(buffer,1);
  data->bytes[2] = 0;
  data->bytes[3] = 1;
  data->data = malloc( size-2 );
  memcpy( data->data, (buffer+2), size-2 );
  data->size = size-2;
  return 0;
}

/**
 * @}
 */

#pragma mark Message size determination
/**
 * @internal
 * Message size determination
 * Determine the size of a message.
 * @{
 */

static int _size_one_byte( struct MIDIMessageData * data, size_t * size ) {
  if( data == NULL || size == NULL ) return 1;
  *size = 1;
  return 0;
}

static int _size_two_bytes( struct MIDIMessageData * data, size_t * size ) {
  if( data == NULL || size == NULL ) return 1;
  *size = 2;
  return 0;
}

static int _size_three_bytes( struct MIDIMessageData * data, size_t * size ) {
  if( data == NULL || size == NULL ) return 1;
  *size = 3;
  return 0;
}

static int _size_system_exclusive( struct MIDIMessageData * data, size_t * size ) {
  if( data == NULL || size == NULL ) return 1;
  if( data->bytes[2] == 0 ) {
    *size = data->size + 2; /* first fragment contains status & manufacturer id */
  } else {
    *size = data->size; /* following fragments contain pure data */
  }
  return 0;
}

/**
 * @}
 */

#pragma mark Message format detectors
/**
 * @internal
 * Message format detectors.
 * Detect the message format using the first byte.
 * @{
 */

static int _test_note_off_on( void * buffer ) {
  return (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_NOTE_OFF<<4)
      || (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_NOTE_ON<<4);
}

static int _test_polyphonic_key_pressure( void * buffer ) {
  return (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_POLYPHONIC_KEY_PRESSURE<<4);
}

static int _test_control_change( void * buffer ) {
  return (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_CONTROL_CHANGE<<4);
}

static int _test_program_change( void * buffer ) {
  return (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_PROGRAM_CHANGE<<4);
}

static int _test_channel_pressure( void * buffer ) {
  return (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_CHANNEL_PRESSURE<<4);
}

static int _test_pitch_wheel_change( void * buffer ) {
  return (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_PITCH_WHEEL_CHANGE<<4);
}

static int _test_system_exclusive( void * buffer ) {
  return VOID_BYTE(buffer,0) == MIDI_STATUS_SYSTEM_EXCLUSIVE;
}

static int _test_time_code_quarter_frame( void * buffer ) {
  return VOID_BYTE(buffer,0) == MIDI_STATUS_TIME_CODE_QUARTER_FRAME;
}

static int _test_song_position_pointer( void * buffer ) {
  return VOID_BYTE(buffer,0) == MIDI_STATUS_SONG_POSITION_POINTER;
}

static int _test_song_select( void * buffer ) {
  return VOID_BYTE(buffer,0) == MIDI_STATUS_SONG_SELECT;
}

static int _test_tune_request( void * buffer ) {
  return VOID_BYTE(buffer,0) == MIDI_STATUS_TUNE_REQUEST;
}

static int _test_real_time( void * buffer ) {
  uint8_t byte = VOID_BYTE(buffer,0);
  if( byte >= MIDI_STATUS_TIMING_CLOCK &&
      byte <= MIDI_STATUS_RESET ) {
    return ( byte != MIDI_STATUS_UNDEFINED2 )
        && ( byte != MIDI_STATUS_UNDEFINED3 );
  }
  return 0;
}

/**
 * @}
 */

#pragma mark Getters and setters
/**
 * @internal
 * Getters and setters.
 * Functions to get ans set properties of the different messages.
 * @{
 */

#define PROPERTY_CASE_BASE(flag,type) \
    case flag: \
      if( size != sizeof(type) ) return 1

#define PROPERTY_CASE_SET(flag,type,field) \
    PROPERTY_CASE_BASE(flag,type); \
      if( ( *((type*)value) & ( (flag==MIDI_STATUS) ? 0xff : 0x7f ) ) != *((type*)value) ) return 1; \
      field = *((type*)value); \
      return 0

#define PROPERTY_CASE_SET_H(flag,type,field) \
    PROPERTY_CASE_BASE(flag,type); \
      if( ( *((type*)value) & ( (flag==MIDI_STATUS) ? 0x0f : 0x07 ) ) != *((type*)value) ) return 1; \
      field = MIDI_NIBBLE_VALUE( *((type*)value), MIDI_LOW_NIBBLE(field) ); \
      return 0

#define PROPERTY_CASE_SET_L(flag,type,field) \
    PROPERTY_CASE_BASE(flag,type); \
      if( ( *((type*)value) & 0x0f ) != *((type*)value) ) return 1; \
      field = MIDI_NIBBLE_VALUE( MIDI_HIGH_NIBBLE(field), *((type*)value) ); \
      return 0

#define PROPERTY_CASE_GET(flag,type,field) \
    PROPERTY_CASE_BASE(flag,type); \
      *((type*)value) = (type) field; \
      return 0

#define PROPERTY_CASE_GET_H(flag,type,field) PROPERTY_CASE_GET(flag,type,MIDI_HIGH_NIBBLE(field))
#define PROPERTY_CASE_GET_L(flag,type,field) PROPERTY_CASE_GET(flag,type,MIDI_LOW_NIBBLE(field))
      
#define PROPERTY_DEFAULT \
    default: \
      return 1

/**
 * Set properties of note on/off messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_note_off_on( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_SET(MIDI_KEY,MIDIKey,m[1]);
    PROPERTY_CASE_SET(MIDI_VELOCITY,MIDIVelocity,m[2]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of note on/off messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_note_off_on( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_GET(MIDI_KEY,MIDIKey,m[1]);
    PROPERTY_CASE_GET(MIDI_VELOCITY,MIDIVelocity,m[2]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of polyphonic key pressure messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_polyphonic_key_pressure( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_SET(MIDI_KEY,MIDIKey,m[1]);
    PROPERTY_CASE_SET(MIDI_PRESSURE,MIDIPressure,m[2]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of polyphonic key pressure messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_polyphonic_key_pressure( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_GET(MIDI_KEY,MIDIKey,m[1]);
    PROPERTY_CASE_GET(MIDI_PRESSURE,MIDIPressure,m[2]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of control change messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_control_change( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_SET(MIDI_CONTROL,MIDIControl,m[1]);
    PROPERTY_CASE_SET(MIDI_VALUE,MIDIValue,m[2]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of control change messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_control_change( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_GET(MIDI_CONTROL,MIDIControl,m[1]);
    PROPERTY_CASE_GET(MIDI_VALUE,MIDIValue,m[2]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of program change messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_program_change( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_SET(MIDI_PROGRAM,MIDIProgram,m[1]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of program change messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_program_change( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_GET(MIDI_PROGRAM,MIDIProgram,m[1]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of channel pressure messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_channel_pressure( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_SET(MIDI_PRESSURE,MIDIPressure,m[1]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of channel pressure messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_channel_pressure( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_GET(MIDI_PRESSURE,MIDIPressure,m[1]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of pitch wheel change messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_pitch_wheel_change( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_SET(MIDI_VALUE_LSB,MIDIValue,m[1]);
    PROPERTY_CASE_SET(MIDI_VALUE_MSB,MIDIValue,m[2]);
    PROPERTY_CASE_BASE(MIDI_VALUE,MIDILongValue);
      m[1] = MIDI_LSB(*(MIDILongValue*)value);
      m[2] = MIDI_MSB(*(MIDILongValue*)value);
      return 0;
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of pitch wheel change messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_pitch_wheel_change( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_GET(MIDI_VALUE_LSB,MIDIValue,m[1]);
    PROPERTY_CASE_GET(MIDI_VALUE_MSB,MIDIValue,m[2]);
    PROPERTY_CASE_BASE(MIDI_VALUE,MIDILongValue);
      *(MIDILongValue*)value = MIDI_LONG_VALUE( m[2], m[1] );
      return 0;
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of system exclusive messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_system_exclusive( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET(MIDI_STATUS,MIDIStatus,data->bytes[0]);
    PROPERTY_CASE_SET(MIDI_MANUFACTURER_ID,MIDIManufacturerId,data->bytes[1]);
    PROPERTY_CASE_SET(MIDI_SYSEX_SIZE,size_t,data->size);
    PROPERTY_CASE_SET(MIDI_SYSEX_FRAGMENT,uint8_t,data->bytes[2]);
    PROPERTY_CASE_BASE(MIDI_SYSEX_DATA,void**);
      if( data->data != NULL && data->bytes[3] == 1 ) free( data->data );
      data->data = *((void**)value);
      return 0;
      break;
  /*case MIDI_SYSEX_DATA:
      if( data->size == 0 || data->data == NULL ) {
        data->data = malloc( size );
      } else {
        data->data = realloc( data->data, size );
      }
      data->size = size;
      memcpy( data->data, value, size );
      return 0;*/
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of system exclusive messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_system_exclusive( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET(MIDI_STATUS,MIDIStatus,data->bytes[0]);
    PROPERTY_CASE_GET(MIDI_MANUFACTURER_ID,MIDIManufacturerId,data->bytes[1]);
    PROPERTY_CASE_GET(MIDI_SYSEX_SIZE,size_t,data->size);
    PROPERTY_CASE_GET(MIDI_SYSEX_FRAGMENT,uint8_t,data->bytes[2]);
    PROPERTY_CASE_BASE(MIDI_SYSEX_DATA,void**);
      *((void**)value) = data->data;
      return 0;
      break;
  /*case MIDI_SYSEX_DATA:
      if( data->size == 0 || data->data == NULL ) return 0;
      memcpy( value, data->data, (size < data->size) ? size : data->size );
      return 0;*/
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of time code quarter frame messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_time_code_quarter_frame( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET_H(MIDI_TIME_CODE_TYPE,MIDIValue,m[1]);
    PROPERTY_CASE_SET_L(MIDI_VALUE,MIDIValue,m[1]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of time code quarter frame messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_time_code_quarter_frame( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET_H(MIDI_TIME_CODE_TYPE,MIDIChannel,m[1]);
    PROPERTY_CASE_GET_L(MIDI_VALUE,MIDIChannel,m[1]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of song position pointer messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_song_position_pointer( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET(MIDI_VALUE_LSB,MIDIValue,m[1]);
    PROPERTY_CASE_SET(MIDI_VALUE_MSB,MIDIValue,m[2]);
    PROPERTY_CASE_BASE(MIDI_VALUE,MIDILongValue);
      m[1] = MIDI_LSB(*(MIDILongValue*)value);
      m[2] = MIDI_MSB(*(MIDILongValue*)value);
      return 0;
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of song position pointer messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_song_position_pointer( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET(MIDI_VALUE_LSB,MIDIValue,m[1]);
    PROPERTY_CASE_GET(MIDI_VALUE_MSB,MIDIValue,m[2]);
    PROPERTY_CASE_BASE(MIDI_VALUE,MIDILongValue);
      *(MIDILongValue*)value = MIDI_LONG_VALUE( m[2], m[1] );
      return 0;
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Set properties of song select messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_song_select( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET(MIDI_VALUE,MIDIValue,m[1]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of song select messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_song_select( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET(MIDI_VALUE,MIDIChannel,m[1]);
    PROPERTY_DEFAULT;
  }
  return 1;
}


/**
 * Set properties of tune request messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_tune_request( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  return 1;
}

/**
 * Set properties of real time messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_real_time( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of tune request and real time messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_tune_request_real_time( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  uint8_t * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

#undef PROPERTY_CASE_BASE
#undef PROPERTY_CASE_SET
#undef PROPERTY_CASE_SET_H
#undef PROPERTY_CASE_SET_L
#undef PROPERTY_CASE_GET
#undef PROPERTY_CASE_GET_H
#undef PROPERTY_CASE_GET_L
#undef PROPERTY_DEFAULT

/**
 * @}
 */

#pragma mark Message format definitions
/**
 * @internal
 * Message format definitions.
 * These definitions hold some message formats.
 * @{
 */

static struct MIDIMessageFormat _note_off_on = {
  &_test_note_off_on,
  &_size_three_bytes,
  &_set_note_off_on,
  &_get_note_off_on,
  &_encode_three_bytes,
  &_decode_three_bytes
};

static struct MIDIMessageFormat _polyphonic_key_pressure = {
  &_test_polyphonic_key_pressure,
  &_size_three_bytes,
  &_set_polyphonic_key_pressure,
  &_get_polyphonic_key_pressure,
  &_encode_three_bytes,
  &_decode_three_bytes
};

static struct MIDIMessageFormat _control_change = {
  &_test_control_change,
  &_size_three_bytes,
  &_set_control_change,
  &_get_control_change,
  &_encode_three_bytes,
  &_decode_three_bytes
};

static struct MIDIMessageFormat _program_change = {
  &_test_program_change,
  &_size_two_bytes,
  &_set_program_change,
  &_get_program_change,
  &_encode_two_bytes,
  &_decode_two_bytes
};

static struct MIDIMessageFormat _channel_pressure = {
  &_test_channel_pressure,
  &_size_two_bytes,
  &_set_channel_pressure,
  &_get_channel_pressure,
  &_encode_two_bytes,
  &_decode_two_bytes
};

static struct MIDIMessageFormat _pitch_wheel_change = {
  &_test_pitch_wheel_change,
  &_size_three_bytes,
  &_set_pitch_wheel_change,
  &_get_pitch_wheel_change,
  &_encode_three_bytes,
  &_decode_three_bytes
};

static struct MIDIMessageFormat _system_exclusive = {
  &_test_system_exclusive,
  &_size_system_exclusive,
  &_set_system_exclusive,
  &_get_system_exclusive,
  &_encode_system_exclusive,
  &_decode_system_exclusive
};

static struct MIDIMessageFormat _time_code_quarter_frame = {
  &_test_time_code_quarter_frame,
  &_size_two_bytes,
  &_set_time_code_quarter_frame,
  &_get_time_code_quarter_frame,
  &_encode_two_bytes,
  &_decode_two_bytes
};

static struct MIDIMessageFormat _song_position_pointer = {
  &_test_song_position_pointer,
  &_size_three_bytes,
  &_set_song_position_pointer,
  &_get_song_position_pointer,
  &_encode_three_bytes,
  &_decode_three_bytes
};

static struct MIDIMessageFormat _song_select = {
  &_test_song_select,
  &_size_two_bytes,
  &_set_song_select,
  &_get_song_select,
  &_encode_two_bytes,
  &_decode_two_bytes
};

static struct MIDIMessageFormat _tune_request = {
  &_test_tune_request,
  &_size_one_byte,
  &_set_tune_request,
  &_get_tune_request_real_time,
  &_encode_one_byte,
  &_decode_one_byte
};

static struct MIDIMessageFormat _real_time = {
  &_test_real_time,
  &_size_one_byte,
  &_set_real_time,
  &_get_tune_request_real_time,
  &_encode_one_byte,
  &_decode_one_byte
};

/**
 * @}
 */

#pragma mark Public functions

#define N_ELEM(a) (sizeof(a) / sizeof(a[0]))

/**
 * @brief Detect the format of message stored in a buffer.
 * Determine the message format used in a stream of bytes.
 * @public @memberof MIDIMessageFormat
 * @param buffer The message as it would appear on a MIDI cable.
 * @return a pointer to the correct message format if the format could be detected.
 * @return a NULL pointer if the format could not be detected.
 */
struct MIDIMessageFormat * MIDIMessageFormatDetect( void * buffer ) {
  static struct MIDIMessageFormat * formats[] = {
    &_note_off_on,
    &_polyphonic_key_pressure,
    &_control_change,
    &_program_change,
    &_channel_pressure,
    &_pitch_wheel_change,
    &_system_exclusive,
    &_time_code_quarter_frame,
    &_song_position_pointer,
    &_song_select,
    &_tune_request,
    &_real_time
  };
  int i;
  for( i=0; i<N_ELEM(formats); i++ ) {
    if( (formats[i]->test)( buffer ) ) {
      return formats[i];
    }
  }
  return NULL;
}

/**
 * @brief Get a format used for a given status.
 * Determine the format that shall be used when accessing messages of
 * a known status.
 * If the status byte looks like a channel status it is correctly
 * shifted so that the correct byte can be checked by MIDIMessageFormatDetect.
 * @see MIDIMessageFormatDetect
 * @public @memberof MIDIMessageFormat
 * @param status A message status.
 * @return a pointer to the correct message format if the format could be detected.
 * @return a NULL pointer if the format could not be detected or if the given
 *         status is not an allowed MIDIStatus.
 */
struct MIDIMessageFormat * MIDIMessageFormatForStatus( MIDIStatus status ) {
  uint8_t byte;
  if( status >= 0x80 ) {
    byte = status;
    if( byte < 0xf0 ) return NULL; /* messed up channel status? */
  } else {
    byte = status << 4;
    if( byte < 0x80 ) return NULL; /* no status bit? */
  }
  return MIDIMessageFormatDetect( &byte );
}

/**
 * @brief Test that the format can be used for a given buffer.
 * Test that the format specified by @c format can be used to
 * decode the bytes in the buffer.
 * @public @memberof MIDIMessageFormat
 * @param format The message format.
 * @param buffer A pointer to the buffer to test.
 * @retval 0 on success.
 * @retval 1 If the format can not be used to access the buffer.
 */
int MIDIMessageFormatTest( struct MIDIMessageFormat * format, void * buffer ) {
  if( format == NULL || format->test == NULL ) return 1;
  return (format->test)( buffer );
}

/**
 * @brief Get the size of a given message.
 * Determine the size of a given message data object using a
 * given message format. This is constant for all messages except
 * system exclusive messages.
 * @public @memberof MIDIMessageFormat
 * @param format The message format.
 * @param data   The message data.
 * @param size   A pointer to the variable in which to store the result.
 * @retval 0 on success.
 * @retval 1 If the size could not be determined.
 */
int MIDIMessageFormatGetSize( struct MIDIMessageFormat * format, struct MIDIMessageData * data, size_t * size ) {
  if( format == NULL || format->size == NULL ) return 1;
  return (format->size)( data, size );
}

/**
 * @brief Set properties.
 * Set properties of messages with a given format.
 * @public @memberof MIDIMessageFormat
 * @param format   The message format.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
int MIDIMessageFormatSet( struct MIDIMessageFormat * format, struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  if( format == NULL || format->set == NULL ) return 1;
  return (format->set)( data, property, size, value );
}

/**
 * @brief Get properties.
 * Get properties of messages with a given format.
 * @public @memberof MIDIMessageFormat
 * @param format   The message format.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
int MIDIMessageFormatGet( struct MIDIMessageFormat * format, struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  if( format == NULL || format->get == NULL ) return 1;
  return (format->get)( data, property, size, value );
}

/**
 * @brief Encode messages.
 * Encode message data objects into a buffer.
 * @public @memberof MIDIMessageFormat
 * @param format   The message format.
 * @param data     The message data object to read from.
 * @param size     The size of the memory pointed to by @c buffer.
 * @param buffer   The buffer to encode the message into.
 * @retval 0 on success.
 * @retval 1 if the message could not be encoded.
 */
int MIDIMessageFormatEncode( struct MIDIMessageFormat * format, struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( format == NULL || format->encode == NULL ) return 1;
  return (format->encode)( data, size, buffer );
}

/**
 * @brief Decode messages.
 * Decode message data objects from a buffer.
 * @public @memberof MIDIMessageFormat
 * @param format   The message format.
 * @param data     The message data object to read from.
 * @param size     The size of the memory pointed to by @c buffer.
 * @param buffer   The buffer to decode the message from.
 * @retval 0 on success.
 * @retval 1 if the message could not be encoded.
 */
int MIDIMessageFormatDecode( struct MIDIMessageFormat * format, struct MIDIMessageData * data, size_t size, void * buffer ) {
  if( format == NULL || format->decode == NULL ) return 1;
  return (format->decode)( data, size, buffer );
}
