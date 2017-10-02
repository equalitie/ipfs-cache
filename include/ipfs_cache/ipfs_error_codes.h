// Error codes returned by IPFS glue code

#pragma once

#ifndef GUARD_ipfs_error_codes_h
#define GUARD_ipfs_error_codes_h

#define IPFS_SUCCESS          0
#define IPFS_RESOLVE_FAILED   1  // failed to resolve IPNS entry
#define IPFS_ADD_FAILED       2  // failed to add data
#define IPFS_CAT_FAILED       3  // failed to get data reader
#define IPFS_READ_FAILED      4  // failed to read data
#define IPFS_PUBLISH_FAILED   5  // failed to publish CID

#endif  // ndef GUARD_ipfs_error_codes_h
