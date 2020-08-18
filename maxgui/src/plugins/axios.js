/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2024-07-16
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
import Vue from 'vue'
import ax from 'axios'
import store from 'store'

const cancelToken = ax.CancelToken

export let cancelSource = cancelToken.source()
export const refreshAxiosToken = () => {
    cancelSource = cancelToken.source()
}
export const cancelAllRequests = () => {
    cancelSource.cancel('Request canceled by user')
}

let apiClient = ax.create({
    baseURL: '/',
    headers: {
        'X-Requested-With': 'XMLHttpRequest',
        'Content-Type': 'application/json',
        'Cache-Control': 'no-cache',
    },
})

apiClient.interceptors.request.use(
    function(config) {
        config.cancelToken = cancelSource.token
        return config
    },
    function(error) {
        return Promise.reject(error)
    }
)

apiClient.interceptors.response.use(
    response => {
        return response
    },
    async error => {
        const { response: { status = null } = {} } = error || {}
        switch (status) {
            case 401:
                await store.dispatch('user/logout')
                break
            case null:
                // request is cancelled by user, so no response is received
                return Promise.reject(error)
            default:
                store.commit('SET_SNACK_BAR_MESSAGE', {
                    text: store.vue.$help.getErrorsArr(error),
                    type: 'error',
                })
                /*
                    When request is dispatched in a modal, an overlay_type loading will be set,
                    Turn it off before returning error
                */
                if (store.state.overlay_type !== null)
                    await store.vue.$help
                        .delay(600)
                        .then(() => store.commit('SET_OVERLAY_TYPE', null))

                return Promise.reject(error)
        }
    }
)

const loginAxios = ax.create({
    baseURL: '/',
    headers: {
        'X-Requested-With': 'XMLHttpRequest',
        'Content-Type': 'application/json',
        'Cache-Control': 'no-cache',
    },
})

// immutable axios instances
Object.defineProperties(Vue.prototype, {
    $axios: {
        get() {
            return apiClient
        },
    },

    $loginAxios: {
        get() {
            return loginAxios
        },
    },
})