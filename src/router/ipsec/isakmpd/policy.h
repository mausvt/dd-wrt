/*	$OpenBSD: policy.h,v 1.12 2003/06/04 07:31:17 ho Exp $	*/
/*	$EOM: policy.h,v 1.12 2000/09/28 12:53:27 niklas Exp $ */

/*
 * Copyright (c) 1999, 2000 Angelos D. Keromytis.  All rights reserved.
 * Copyright (c) 2000 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _POLICY_H_
#define _POLICY_H_

#if defined (USE_KEYNOTE)
#define CREDENTIAL_FILE "credentials"
#define PRIVATE_KEY_FILE "private_key"
#endif

extern int keynote_sessid;
extern int keynote_policy_asserts_num;
extern int x509_policy_asserts_num;
extern int x509_policy_asserts_num_alloc;
extern char **keynote_policy_asserts;
extern char **x509_policy_asserts;
extern struct exchange *policy_exchange;
extern struct sa *policy_sa;
extern struct sa *policy_isakmp_sa;

extern void policy_init (void);
extern char *policy_callback (char *);
extern int keynote_cert_init (void);
extern void *keynote_cert_get (u_int8_t *, u_int32_t);
extern int keynote_cert_validate (void *);
extern int keynote_cert_insert (int, void *);
extern void keynote_cert_free (void *);
extern int keynote_certreq_validate (u_int8_t *, u_int32_t);
extern void *keynote_certreq_decode (u_int8_t *, u_int32_t);
extern void keynote_free_aca (void *);
extern int keynote_cert_obtain (u_int8_t *, size_t, void *,
				u_int8_t **, u_int32_t *);
extern int keynote_cert_get_subjects (void *, int *, u_int8_t ***,
				      u_int32_t **);
extern int keynote_cert_get_key (void *, void *);
extern void *keynote_cert_dup (void *);
extern void keynote_serialize (void *, u_int8_t **, u_int32_t *);
extern char *keynote_printable (void *);
extern void *keynote_from_printable (char *);
#endif /* _POLICY_H_ */
