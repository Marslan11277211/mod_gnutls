/**
 *  Copyright 2004-2005 Paul Querna
 *  Copyright 2014 Nikos Mavrogiannopoulos
 *  Copyright 2015-2016 Thomas Klute
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef __MOD_GNUTLS_CACHE_H__
#define __MOD_GNUTLS_CACHE_H__

#include "mod_gnutls.h"
#include <httpd.h>

/**
 * Init the Cache after Configuration is done
 */
int mgs_cache_post_config(apr_pool_t *p, server_rec *s, mgs_srvconf_rec *sc);

/**
 * Init the Cache inside each Process
 */
int mgs_cache_child_init(apr_pool_t *p, server_rec *s, mgs_srvconf_rec *sc);

/**
 * Setup the Session Caching
 */
int mgs_cache_session_init(mgs_handle_t *ctxt);

#define GNUTLS_SESSION_ID_STRING_LEN \
    ((GNUTLS_MAX_SESSION_ID + 1) * 2)



/**
 * Convert a SSL Session ID into a Null Terminated Hex Encoded String
 * @param id raw SSL Session ID
 * @param idlen Length of the raw Session ID
 * @param str Location to store the Hex Encoded String
 * @param strsize The Maximum Length that can be stored in str
 */
char *mgs_session_id2sz(unsigned char *id, int idlen, char *str, int strsize);

/**
 * Convert a time_t into a Null Terminated String
 * @param t time_t time
 * @param str Location to store the Hex Encoded String
 * @param strsize The Maximum Length that can be stored in str
 */
char *mgs_time2sz(time_t t, char *str, int strsize);

#endif /** __MOD_GNUTLS_CACHE_H__ */