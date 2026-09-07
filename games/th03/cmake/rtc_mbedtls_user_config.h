#ifndef TH03_RTC_MBEDTLS_USER_CONFIG_H
#define TH03_RTC_MBEDTLS_USER_CONFIG_H

// libdatachannel's DTLS transport negotiates an SRTP protection profile even
// when its optional media stack is disabled. Mbed TLS leaves the extension
// off by default, so enable the exact feature required by that transport.
#define MBEDTLS_SSL_DTLS_SRTP

#endif
