#	$OpenBSD: Makefile,v 1.14 2003/06/03 14:39:51 ho Exp $
#	$EOM: Makefile,v 1.16 2000/09/28 12:53:27 niklas Exp $

#
# Copyright (c) 1999 Niels Provos.  All rights reserved.
# Copyright (c) 1999, 2001 Niklas Hallqvist.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# This code was written under funding by Ericsson Radio Systems.
#

# Test X509

# Enable this if you have a DNSSEC enabled OpenSSL
#LIBLWRES=	/usr/local/lib/liblwres.a

PROG=		x509test
SRCS=		x509test.c conf.c log.c libcrypto.c sysdep.c field.c util.c \
		isakmp_fld.c ipsec_fld.c ipsec_num.c isakmp_num.c constants.c \
		cert.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
FEATURES!=	awk '/^FEATURES=/ { print $$0 }' ${.CURDIR}/../../Makefile | sed 's/FEATURES=.//'
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall \
		-DUSE_X509
NOMAN=
DEBUG=		-g

.if ${FEATURES:Mgmp} == "gmp"
CFLAGS+=	-DMP_FLAVOUR=MP_FLAVOUR_GMP
LDADD+=		-lgmp
DPADD+=		${LIBGMP}
.else
CFLAGS+=	-DMP_FLAVOUR=MP_FLAVOUR_OPENSSL
.endif

.include "${TOPSRC}/sysdep/${OS}/Makefile.sysdep"

.ifdef HAVE_DLOPEN
X509=		x509.c
POLICY=		policy.c
CFLAGS+=	-DHAVE_DLOPEN
SRCS+=		dyn.c
.endif

.ifdef USE_KEYNOTE
USE_LIBCRYPTO=	yes
POLICY=		policy.c
LDADD+=		-lkeynote -lm
DPADD+=		${LIBKEYNOTE} ${LIBM}
CFLAGS+=	-DUSE_KEYNOTE
.endif

.ifdef USE_LIBCRYPTO
X509=		x509.c
CFLAGS+=	-DUSE_LIBCRYPTO
LDADD+=		-lcrypto ${LIBLWRES}
DPADD+=		${LIBCRYPTO}
.endif

.if !defined (HAVE_DLOPEN) && !defined (USE_LIBCRYPTO) || !defined (USE_KEYNOTE)
.BEGIN:

PROG=
.endif

SRCS+=		${X509} ${POLICY}

LDADD+=		${DESLIB}
DPADD+=		${DESLIBDEP}

.include <bsd.prog.mk>
