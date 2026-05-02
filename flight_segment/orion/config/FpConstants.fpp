# ======================================================================
# Project-level overrides for F-Prime framework constants.
#
# Only constants that differ from the framework defaults are listed here.
# The CONFIGURATION_OVERRIDES directive in CMakeLists.txt replaces the
# framework's FpConstants.fpp with this file, so every constant defined
# in the original must be repeated (the FPP compiler sees only one copy).
# ======================================================================

# ---------------------------------------------------------------------
# Buffer sizes
# ---------------------------------------------------------------------

constant FW_OBJ_SIMPLE_REG_BUFF_SIZE = 255
constant FW_QUEUE_NAME_BUFFER_SIZE = 80
constant FW_TASK_NAME_BUFFER_SIZE = 80

@ Bumped from 512 to 768 so that the log buffer can hold the full VLM
@ reason string plus event metadata without tripping the CCSDS TmFramer
@ static_assert (TmPayloadCapacity = 1016, needs >= COM + 13).
constant FW_COM_BUFFER_MAX_SIZE = 768

constant FW_SM_SIGNAL_BUFFER_MAX_SIZE = 128
constant FW_CMD_ARG_BUFFER_MAX_SIZE = FW_COM_BUFFER_MAX_SIZE - sizeof(FwOpcodeType) - sizeof(FwPacketDescriptorType)
constant FW_CMD_STRING_MAX_SIZE = 40
constant FW_LOG_BUFFER_MAX_SIZE = FW_COM_BUFFER_MAX_SIZE - sizeof(FwEventIdType) - sizeof(FwPacketDescriptorType)

@ Bumped from 200 to 400 so VlmInferenceEngine's reason field is not
@ truncated in GDS event logs.
constant FW_LOG_STRING_MAX_SIZE = 400

constant FW_TLM_BUFFER_MAX_SIZE = FW_COM_BUFFER_MAX_SIZE - sizeof(FwChanIdType) - sizeof(FwPacketDescriptorType)
constant FW_STATEMENT_ARG_BUFFER_MAX_SIZE = FW_CMD_ARG_BUFFER_MAX_SIZE
constant FW_TLM_STRING_MAX_SIZE = 40
constant FW_PARAM_BUFFER_MAX_SIZE = FW_COM_BUFFER_MAX_SIZE - sizeof(FwPrmIdType) - sizeof(FwPacketDescriptorType)
constant FW_PARAM_STRING_MAX_SIZE = 40
constant FW_FILE_BUFFER_MAX_SIZE = FW_COM_BUFFER_MAX_SIZE
constant FW_INTERNAL_INTERFACE_STRING_MAX_SIZE = 256

@ Bumped from 256 to 600 to fit the formatted VLM event text.
constant FW_LOG_TEXT_BUFFER_SIZE = 600

@ Bumped from 256 to 400 (must be >= FW_LOG_STRING_MAX_SIZE per static_assert).
constant FW_FIXED_LENGTH_STRING_SIZE = 400

# ---------------------------------------------------------------------
# Other constants
# ---------------------------------------------------------------------

constant FW_OBJ_SIMPLE_REG_ENTRIES = 500
constant FW_QUEUE_SIMPLE_QUEUE_ENTRIES = 100
constant FW_ASSERT_COUNT_MAX = 10
constant FW_CONTEXT_DONT_CARE = 0xFF
dictionary constant FW_SERIALIZE_TRUE_VALUE = 0xFF
dictionary constant FW_SERIALIZE_FALSE_VALUE = 0x00
