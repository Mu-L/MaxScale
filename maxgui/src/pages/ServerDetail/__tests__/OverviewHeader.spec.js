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

import { expect } from 'chai'
import mount from '@tests/unit/setup'
import OverviewHeader from '@/pages/ServerDetail/OverviewHeader'
import { dummy_all_servers } from '@tests/unit/utils'

const propsMountFactory = props =>
    mount({
        shallow: false,
        component: OverviewHeader,
        props: props,
    })

describe('ServerDetail - OverviewHeader', () => {
    let wrapper
    beforeEach(async () => {
        wrapper = mount({
            shallow: false,
            component: OverviewHeader,
            props: {
                currentServer: dummy_all_servers[0],
            },
        })
    })

    describe('outlined-overview-card render counts assertions', async () => {
        // 6 means when server is using port, 5 is when server is using socket
        const countCases = [6, 5]

        countCases.forEach(count => {
            let des = `Should render ${count} outlined-overview-card components`
            if (count === 6) des += 'if server is using port'
            else des += 'if server is using socket'
            it(des, async () => {
                let currentServer = wrapper.vm.$help.lodash.cloneDeep(dummy_all_servers[0])
                currentServer.attributes.parameters = {
                    address: '127.0.0.1',
                    port: count === 6 ? 4001 : null,
                    socket: count === 6 ? null : 'tmp/maxscale.sock',
                }
                wrapper = propsMountFactory({
                    currentServer: currentServer,
                })

                const outlineOverviewCards = wrapper.findAllComponents({
                    name: 'outlined-overview-card',
                })
                expect(outlineOverviewCards.length).to.be.equals(count)
            })
        })
    })

    it(`Should automatically assign 'undefined' string if attribute is not defined`, async () => {
        let currentServer = wrapper.vm.$help.lodash.cloneDeep(dummy_all_servers[0])
        delete currentServer.attributes.last_event
        delete currentServer.attributes.triggered_at

        wrapper = propsMountFactory({
            currentServer: currentServer,
        })
        const { last_event, triggered_at } = wrapper.vm.getTopOverviewInfo

        expect(last_event).to.be.equals('undefined')
        expect(triggered_at).to.be.equals('undefined')
    })

    describe('Should get accurate keys', async () => {
        // 6 means when server is using port, 5 is when server is using socket
        const countCases = [6, 5]

        countCases.forEach(count => {
            let des = `Should show `
            let expectKeys = []
            if (count === 6) {
                expectKeys = ['address', 'port', 'state', 'last_event', 'triggered_at', 'monitor']
            } else {
                des += 'if server is using socket'
                expectKeys = ['socket', 'state', 'last_event', 'triggered_at', 'monitor']
            }
            des += expectKeys.join(', ')

            it(des, async () => {
                let currentServer = wrapper.vm.$help.lodash.cloneDeep(dummy_all_servers[0])
                currentServer.attributes.parameters = {
                    address: '127.0.0.1',
                    port: count === 6 ? 4001 : null,
                    socket: count === 6 ? null : 'tmp/maxscale.sock',
                }
                wrapper = propsMountFactory({
                    currentServer: currentServer,
                })
                const getTopOverviewInfo = wrapper.vm.getTopOverviewInfo
                expect(Object.keys(getTopOverviewInfo)).to.be.deep.equals(expectKeys)
            })
        })
    })
})
