/*
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2010  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <ofono.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/plugin.h>
#include <ofono/log.h>
#include <ofono/modem.h>
#include <gdbus.h>

#include "bluetooth.h"

#define HFP_AG_CHANNEL	13

static struct server *server;
static guint modemwatch_id;
static GList *modems;

static const gchar *hfp_ag_record =
"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
"<record>\n"
"  <attribute id=\"0x0001\">\n"
"    <sequence>\n"
"      <uuid value=\"0x111F\"/>\n"
"      <uuid value=\"0x1203\"/>\n"
"    </sequence>\n"
"  </attribute>\n"
"\n"
"  <attribute id=\"0x0004\">\n"
"    <sequence>\n"
"      <sequence>\n"
"        <uuid value=\"0x0100\"/>\n"
"      </sequence>\n"
"      <sequence>\n"
"        <uuid value=\"0x0003\"/>\n"
"        <uint8 value=\"13\" name=\"channel\"/>\n"
"      </sequence>\n"
"    </sequence>\n"
"  </attribute>\n"
"\n"
"  <attribute id=\"0x0009\">\n"
"    <sequence>\n"
"      <sequence>\n"
"        <uuid value=\"0x111E\"/>\n"
"        <uint16 value=\"0x0105\" name=\"version\"/>\n"
"      </sequence>\n"
"    </sequence>\n"
"  </attribute>\n"
"\n"
"  <attribute id=\"0x0100\">\n"
"    <text value=\"Hands-Free Audio Gateway\" name=\"name\"/>\n"
"  </attribute>\n"
"\n"
"  <attribute id=\"0x0301\">\n"
"    <uint8 value=\"0x01\" />\n"
"  </attribute>\n"
"\n"
"  <attribute id=\"0x0311\">\n"
"    <uint16 value=\"0x0000\" />\n"
"  </attribute>\n"
"</record>\n";

static void hfp_ag_connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
	struct ofono_modem *modem;
	struct ofono_emulator *em;
	int fd;

	DBG("");

	if (err) {
		DBG("%s", err->message);
		goto failed;
	}

	/* Pick the first voicecall capable modem */
	modem = modems->data;
	if (modem == NULL)
		goto failed;

	DBG("Picked modem %p for emulator", modem);

	em = ofono_emulator_create(modem, OFONO_EMULATOR_TYPE_HFP);
	if (em == NULL)
		goto failed;

	fd = g_io_channel_unix_get_fd(io);
	ofono_emulator_register(em, fd);

	g_io_channel_set_close_on_unref(io, FALSE);

	return;

failed:
	g_io_channel_shutdown(io, TRUE, NULL);
}

static void voicecall_watch(struct ofono_atom *atom,
				enum ofono_atom_watch_condition cond,
				void *data)
{
	struct ofono_modem *modem = data;

	if (cond == OFONO_ATOM_WATCH_CONDITION_REGISTERED) {
		modems = g_list_append(modems, modem);

		if (modems->next == NULL)
			server = bluetooth_register_server(HFP_AG_CHANNEL,
							hfp_ag_record,
							hfp_ag_connect_cb,
							NULL);
	} else {
		modems = g_list_remove(modems, modem);
		if (modems == NULL &&  server != NULL) {
			bluetooth_unregister_server(server);
			server = NULL;
		}
	}
}

static void modem_watch(struct ofono_modem *modem, gboolean added, void *user)
{
	DBG("modem: %p, added: %d", modem, added);

	if (added == FALSE)
		return;

	__ofono_modem_add_atom_watch(modem, OFONO_ATOM_TYPE_VOICECALL,
					voicecall_watch, modem, NULL);
}

static void call_modemwatch(struct ofono_modem *modem, void *user)
{
	modem_watch(modem, TRUE, user);
}

static int hfp_ag_init()
{
	modemwatch_id = __ofono_modemwatch_add(modem_watch, NULL, NULL);
	__ofono_modem_foreach(call_modemwatch, NULL);

	return 0;
}

static void hfp_ag_exit()
{
	__ofono_modemwatch_remove(modemwatch_id);

	if (server) {
		bluetooth_unregister_server(server);
		server = NULL;
	}
}

OFONO_PLUGIN_DEFINE(hfp_ag, "Hands-Free Audio Gateway Profile Plugins", VERSION,
			OFONO_PLUGIN_PRIORITY_DEFAULT, hfp_ag_init, hfp_ag_exit)
