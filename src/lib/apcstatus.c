/*
 * apcstatus.c
 *
 * Output STATUS information.
 */

/*
 * Copyright (C) 1999-2005 Kern Sibbald
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General
 * Public License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include "apc.h"

/* Send the full status of the UPS to the client. */
int output_status(UPSINFO *ups, int sockfd,
   void s_open(UPSINFO *ups),
   void s_write(UPSINFO *ups, char *fmt, ...),
   int s_close(UPSINFO *ups, int sockfd))
{
   char datetime[MAXSTRING];
   char buf[MAXSTRING];
   time_t now = time(NULL);
   int time_on_batt;
   struct tm tm;
   char status[100];

   s_open(ups);

   if (ups->poll_time == 0)        /* this is always zero on slave */
      ups->poll_time = now;

   localtime_r(&ups->poll_time, &tm);
   strftime(datetime, 100, "%a %b %d %X %Z %Y", &tm);

   /* put the last UPS poll time on the DATE record */
   s_write(ups, "DATE     : %s\n", datetime);

   gethostname(buf, sizeof buf);
   s_write(ups, "HOSTNAME : %s\n", buf);
   s_write(ups, "RELEASE  : %s\n", ups->release);
   s_write(ups, "VERSION  : " APCUPSD_RELEASE " (" ADATE ") " APCUPSD_HOST "\n");

   if (*ups->upsname)
      s_write(ups, "UPSNAME  : %s\n", ups->upsname);

   s_write(ups, "CABLE    : %s\n", ups->cable.long_name);
   s_write(ups, "MODEL    : %s\n", ups->mode.long_name);
   s_write(ups, "UPSMODE  : %s\n", ups->upsclass.long_name);

   localtime_r(&ups->start_time, &tm);
   strftime(datetime, 100, "%a %b %d %X %Z %Y", &tm);
   s_write(ups, "STARTTIME: %s\n", datetime);

   if (ups->sharenet.type != DISABLE)
      s_write(ups, "SHARE    : %s\n", ups->sharenet.long_name);

   /* If slave, send last update time/date from master */
   if (ups->is_slave()) {    /* we must be a slave */
      if (ups->last_master_connect_time == 0) {
         s_write(ups, "MASTERUPD: No connection to Master\n");
      } else {
         localtime_r(&ups->last_master_connect_time, &tm);
         strftime(datetime, 100, "%a %b %d %X %Z %Y", &tm);
         s_write(ups, "MASTERUPD: %s\n", datetime);
      }

      s_write(ups, "MASTER   : %s\n", ups->master_name);
   }

   switch (ups->mode.type) {
   case BK:
   case SHAREBASIC:
   case NETUPS:
      if (!ups->is_onbatt()) {
         s_write(ups, "LINEFAIL : OK\n");
         s_write(ups, "BATTSTAT : OK\n");
      } else {
         s_write(ups, "LINEFAIL : DOWN\n");

         if (!ups->is_battlow())
            s_write(ups, "BATTSTAT : RUNNING\n");
         else
            s_write(ups, "BATTSTAT : FAILING\n");
      }

      s_write(ups, "STATFLAG : 0x%08X Status Flag\n", ups->Status);
      break;

   case BKPRO:
   case VS:
      if (!ups->is_onbatt()) {
         s_write(ups, "LINEFAIL : OK\n");
         s_write(ups, "BATTSTAT : OK\n");
         if (ups->is_boost())
            s_write(ups, "MAINS    : LOW\n");
         else if (ups->is_trim())
            s_write(ups, "MAINS    : HIGH\n");
         else
            s_write(ups, "MAINS    : OK\n");
      } else {
         s_write(ups, "LINEFAIL : DOWN\n");
         if (!ups->is_battlow())
            s_write(ups, "BATTSTAT : RUNNING\n");
         else
            s_write(ups, "BATTSTAT : FAILING\n");
      }

      /*
       * If communication is lost the only valid flag at this point
       * is UPS_commlost and all the other flags are possibly wrong.
       * For this reason the old code used to override UPS_online zeroing
       * it for printing purposes.
       * But this needs more careful checking and may be this is not what
       * we want. After all when UPS_commlost is asserted the other flags
       * contain interesting information about the last known state of UPS
       * and Kern's intention is to use them to infer the next action of
       * a disconnected slave. Personally I think this can be true also
       * when apcupsd loses communication over any other kind of transport
       * like serial or usb or others.
       */
      s_write(ups, "STATFLAG : 0x%08X Status Flag\n", ups->Status);

      /* Note! Fall through is wanted */

   case NBKPRO:
   case SMART:
   case SHARESMART:
   case MATRIX:
   case APCSMART_UPS:
   case USB_UPS:
   case TEST_UPS:
   case DUMB_UPS:
   case NETWORK_UPS:
   case SNMP_UPS:
      status[0] = 0;

      /* Now output human readable form */
      if (ups->is_calibration())
         astrncat(status, "CAL ", sizeof(status));

      if (ups->is_trim())
         astrncat(status, "TRIM ", sizeof(status));

      if (ups->is_boost())
         astrncat(status, "BOOST ", sizeof(status));

      if (ups->is_online())
         astrncat(status, "ONLINE ", sizeof(status));

      if (ups->is_onbatt())
         astrncat(status, "ONBATT ", sizeof(status));

      if (ups->is_overload())
         astrncat(status, "OVERLOAD ", sizeof(status));

      if (ups->is_battlow())
         astrncat(status, "LOWBATT ", sizeof(status));

      if (ups->is_replacebatt())
         astrncat(status, "REPLACEBATT ", sizeof(status));

      if (!ups->is_battpresent())
         astrncat(status, "NOBATT ", sizeof(status));

      if (ups->is_slave())
         astrncat(status, "SLAVE ", sizeof(status));

      if (ups->is_slavedown())
         astrncat(status, "SLAVEDOWN", sizeof(status));

      /* These override the above */
      if (ups->is_commlost())
         astrncpy(status, "COMMLOST ", sizeof(status));

      if (ups->is_shutdown())
         astrncpy(status, "SHUTTING DOWN", sizeof(status));

      s_write(ups, "STATUS   : %s\n", status);

      if (ups->UPS_Cap[CI_VLINE])
         s_write(ups, "LINEV    : %05.1f Volts\n", ups->LineVoltage);
      else
         Dmsg0(20, "NO CI_VLINE\n");

      if (ups->UPS_Cap[CI_LOAD])
         s_write(ups, "LOADPCT  : %5.1f Percent Load Capacity\n", ups->UPSLoad);

      if (ups->UPS_Cap[CI_BATTLEV])
         s_write(ups, "BCHARGE  : %05.1f Percent\n", ups->BattChg);

      if (ups->UPS_Cap[CI_RUNTIM])
         s_write(ups, "TIMELEFT : %5.1f Minutes\n", ups->TimeLeft);

      s_write(ups, "MBATTCHG : %d Percent\n", ups->percent);
      s_write(ups, "MINTIMEL : %d Minutes\n", ups->runtime);
      s_write(ups, "MAXTIME  : %d Seconds\n", ups->maxtime);

      if (ups->UPS_Cap[CI_VMAX])
         s_write(ups, "MAXLINEV : %05.1f Volts\n", ups->LineMax);

      if (ups->UPS_Cap[CI_VMIN])
         s_write(ups, "MINLINEV : %05.1f Volts\n", ups->LineMin);

      if (ups->UPS_Cap[CI_VOUT])
         s_write(ups, "OUTPUTV  : %05.1f Volts\n", ups->OutputVoltage);

      if (ups->UPS_Cap[CI_SENS]) {
         switch ((*ups).sensitivity[0]) {
         case 'A':                /* Matrix only */
            s_write(ups, "SENSE    : Auto Adjust\n");
            break;
         case 'L':
            s_write(ups, "SENSE    : Low\n");
            break;
         case 'M':
            s_write(ups, "SENSE    : Medium\n");
            break;
         case 'H':
            s_write(ups, "SENSE    : High\n");
            break;
         default:
            s_write(ups, "SENSE    : Unknown\n");
            break;
         }
      }

      if (ups->UPS_Cap[CI_DWAKE])
         s_write(ups, "DWAKE    : %03d Seconds\n", ups->dwake);

      if (ups->UPS_Cap[CI_DSHUTD])
         s_write(ups, "DSHUTD   : %03d Seconds\n", ups->dshutd);

      if (ups->UPS_Cap[CI_DLBATT])
         s_write(ups, "DLOWBATT : %02d Minutes\n", ups->dlowbatt);

      if (ups->UPS_Cap[CI_LTRANS])
         s_write(ups, "LOTRANS  : %03d.0 Volts\n", ups->lotrans);

      if (ups->UPS_Cap[CI_HTRANS])
         s_write(ups, "HITRANS  : %03d.0 Volts\n", ups->hitrans);

      if (ups->UPS_Cap[CI_RETPCT])
         s_write(ups, "RETPCT   : %03d.0 Percent\n", ups->rtnpct);

      if (ups->UPS_Cap[CI_ITEMP])
         s_write(ups, "ITEMP    : %04.1f C Internal\n", ups->UPSTemp);

      if (ups->UPS_Cap[CI_DALARM]) {
         switch ((*ups).beepstate[0]) {
         case 'T':
            s_write(ups, "ALARMDEL : 30 seconds\n");
            break;
         case 'L':
            s_write(ups, "ALARMDEL : Low Battery\n");
            break;
         case 'N':
            s_write(ups, "ALARMDEL : No alarm\n");
            break;
         case '0':
            s_write(ups, "ALARMDEL : 5 seconds\n");
            break;
         default:
            s_write(ups, "ALARMDEL : Always\n");
            break;
         }
      }

      if (ups->UPS_Cap[CI_VBATT])
         s_write(ups, "BATTV    : %04.1f Volts\n", ups->BattVoltage);

      if (ups->UPS_Cap[CI_FREQ])
         s_write(ups, "LINEFREQ : %03.1f Hz\n", ups->LineFreq);

      /* Output cause of last transfer to batteries */
      switch (ups->lastxfer) {
      case XFER_NONE:
         s_write(ups, "LASTXFER : No transfers since turnon\n");
         break;
      case XFER_SELFTEST:
         s_write(ups, "LASTXFER : Automatic or explicit self test\n");
         break;
      case XFER_FORCED:
         s_write(ups, "LASTXFER : Forced by software\n");
         break;
      case XFER_UNDERVOLT:
         s_write(ups, "LASTXFER : Low line voltage\n");
         break;
      case XFER_OVERVOLT:
         s_write(ups, "LASTXFER : High line voltage\n");
         break;
      case XFER_RIPPLE:
         s_write(ups, "LASTXFER : Unacceptable line voltage changes\n");
         break;
      case XFER_NOTCHSPIKE:
         s_write(ups, "LASTXFER : Line voltage notch or spike\n");
         break;
      case XFER_UNKNOWN:
         s_write(ups, "LASTXFER : UNKNOWN EVENT\n");
         break;
      case XFER_NA:
      default:
         /* UPS doesn't report this data */
         break;
      }

      s_write(ups, "NUMXFERS : %d\n", ups->num_xfers);
      if (ups->num_xfers > 0) {
         localtime_r(&ups->last_onbatt_time, &tm);
         strftime(datetime, 100, "%a %b %d %X %Z %Y", &tm);
         s_write(ups, "XONBATT  : %s\n", datetime);
      }

      if (ups->is_onbatt() && ups->last_onbatt_time > 0)
         time_on_batt = now - ups->last_onbatt_time;
      else
         time_on_batt = 0;
      s_write(ups, "TONBATT  : %d seconds\n", time_on_batt);
      s_write(ups, "CUMONBATT: %d seconds\n", ups->cum_time_on_batt + time_on_batt);

      if (ups->last_offbatt_time > 0) {
         localtime_r(&ups->last_offbatt_time, &tm);
         strftime(datetime, 100, "%a %b %d %X %Z %Y", &tm);
         s_write(ups, "XOFFBATT : %s\n", datetime);
      } else {
         s_write(ups, "XOFFBATT : N/A\n");
      }

      if (ups->LastSelfTest != 0) {
         localtime_r(&ups->LastSelfTest, &tm);
         strftime(datetime, 100, "%a %b %d %X %Z %Y", &tm);
         s_write(ups, "LASTSTEST: %s\n", datetime);
      }

      /* Status of last self test */
      switch (ups->testresult) {
      case TEST_NONE:
         s_write(ups, "SELFTEST : NO\n");
         break;
      case TEST_FAILED:
         s_write(ups, "SELFTEST : NG\n");
         break;
      case TEST_WARNING:
         s_write(ups, "SELFTEST : WN\n");
         break;
      case TEST_INPROGRESS:
         s_write(ups, "SELFTEST : IP\n");
         break;
      case TEST_PASSED:
         s_write(ups, "SELFTEST : OK\n");
         break;
      case TEST_FAILCAP:
         s_write(ups, "SELFTEST : BT\n");
         break;
      case TEST_FAILLOAD:
         s_write(ups, "SELFTEST : NG\n");
         break;
      case TEST_UNKNOWN:
         s_write(ups, "SELFTEST : ??\n");
         break;
      case TEST_NA:
      default:
         /* UPS doesn't report this data */
         break;
      }

      /* Self test interval */
      if (ups->UPS_Cap[CI_STESTI])
         s_write(ups, "STESTI   : %s\n", ups->selftest);

      /* output raw bits */
      s_write(ups, "STATFLAG : 0x%08X Status Flag\n", ups->Status);

      if (ups->UPS_Cap[CI_DIPSW])
         s_write(ups, "DIPSW    : 0x%02X Dip Switch\n", ups->dipsw);

      if (ups->UPS_Cap[CI_REG1])
         s_write(ups, "REG1     : 0x%02X Register 1\n", ups->reg1);

      if (ups->UPS_Cap[CI_REG2])
         s_write(ups, "REG2     : 0x%02X Register 2\n", ups->reg2);

      if (ups->UPS_Cap[CI_REG3])
         s_write(ups, "REG3     : 0x%02X Register 3\n", ups->reg3);

      if (ups->UPS_Cap[CI_MANDAT])
         s_write(ups, "MANDATE  : %s\n", ups->birth);

      if (ups->UPS_Cap[CI_SERNO])
         s_write(ups, "SERIALNO : %s\n", ups->serial);

      if (ups->UPS_Cap[CI_BATTDAT] || ups->UPS_Cap[CI_BattReplaceDate])
         s_write(ups, "BATTDATE : %s\n", ups->battdat);

      if (ups->UPS_Cap[CI_NOMOUTV])
         s_write(ups, "NOMOUTV  : %03d\n", ups->NomOutputVoltage);

      if (ups->UPS_Cap[CI_NOMBATTV])
         s_write(ups, "NOMBATTV : %5.1f\n", ups->nombattv);

      if (ups->UPS_Cap[CI_HUMID])
         s_write(ups, "HUMIDITY : %5.1f\n", ups->humidity);

      if (ups->UPS_Cap[CI_ATEMP])
         s_write(ups, "AMBTEMP  : %5.1f\n", ups->ambtemp);

      if (ups->UPS_Cap[CI_EXTBATTS])
         s_write(ups, "EXTBATTS : %d\n", ups->extbatts);

      if (ups->UPS_Cap[CI_BADBATTS])
         s_write(ups, "BADBATTS : %d\n", ups->badbatts);

      if (ups->UPS_Cap[CI_REVNO])
         s_write(ups, "FIRMWARE : %s\n", ups->firmrev);

      if (ups->UPS_Cap[CI_UPSMODEL])
         s_write(ups, "APCMODEL : %s\n", ups->upsmodel);

      break;

   default:
      break;
   }

   /* put the current time in the END APC record */
   localtime_r(&now, &tm);
   strftime(datetime, 100, "%a %b %d %X %Z %Y", &tm);
   s_write(ups, "END APC  : %s\n", datetime);

   return s_close(ups, sockfd);
}

/*
 * stat_open(), stat_close(), and stat_write() are called
 * by output_status() to open,close, and write the status
 * report.
 */
void stat_open(UPSINFO *ups)
{
}

int stat_close(UPSINFO *ups, int fd)
{
   return 0;
}

void stat_print(UPSINFO *ups, char *fmt, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, fmt);
   vfprintf(stdout, (char *)fmt, arg_ptr);
   va_end(arg_ptr);
}

#ifdef HAVE_CYGWIN

#include <windows.h>

/*
 * What follows are routines that are called by the Windows
 * C++ routines to get status information.
 * They are used for the info bubble on the system tray.
 */

extern int shm_OK;
static char buf[MAXSTRING];
int battstat = 0;
static HWND dlg_hwnd = NULL;
static int dlg_idlist = 0;

/*
 * Return a UPS status string. For this cut, we return simply
 * the Online/OnBattery status. The stat flag is intended to
 * be used to retrieve additional status strings.
 */
char *ups_status(int stat)
{
   UPSINFO *ups = getNextUps(NULL);

   if (!shm_OK) {
      battstat = 0;
      astrncpy(buf, "not initialized", sizeof(buf));
      return buf;
   }

   if (!ups->is_onbatt())
      battstat = 100;
   else
      battstat = 0;

   astrncpy(buf, "Status not available", sizeof(buf));

   switch (ups->mode.type) {
   case BK:
   case SHAREBASIC:
   case NETUPS:
   case BKPRO:
   case VS:
      if (!ups->is_onbatt()) {
         astrncpy(buf, "ONLINE", sizeof(buf));
      } else {
         astrncpy(buf, "ON BATTERY", sizeof(buf));
      }
      break;

   case NBKPRO:
   case SMART:
   case SHARESMART:
   case MATRIX:
   case APCSMART_UPS:
   case USB_UPS:
   case TEST_UPS:
   case DUMB_UPS:
   case NETWORK_UPS:
   case SNMP_UPS:
      buf[0] = 0;

      /* Now output human readable form */
      if (ups->is_calibration())
         astrncat(buf, "CAL ", sizeof(buf));
      if (ups->is_trim())
         astrncat(buf, "TRIM ", sizeof(buf));
      if (ups->is_boost())
         astrncat(buf, "BOOST ", sizeof(buf));
      if (ups->is_online())
         astrncat(buf, "ONLINE ", sizeof(buf));
      if (ups->is_onbatt())
         astrncat(buf, "ON BATTERY ", sizeof(buf));
      if (ups->is_overload())
         astrncat(buf, "OVERLOAD ", sizeof(buf));
      if (ups->is_battlow())
         astrncat(buf, "LOWBATT ", sizeof(buf));
      if (ups->is_replacebatt())
         astrncat(buf, "REPLACEBATT ", sizeof(buf));
      if (!ups->is_onbatt() && ups->UPS_Cap[CI_BATTLEV])
         battstat = (int)ups->BattChg;
      break;
   }

   return buf;
}


static void stat_list(UPSINFO *ups, char *fmt, ...)
{
   va_list arg_ptr;
   int len;

   va_start(arg_ptr, fmt);
   avsnprintf(buf, sizeof(buf), (char *)fmt, arg_ptr);
   va_end(arg_ptr);
   len = strlen(buf);

   while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
      buf[--len] = 0;

   SendDlgItemMessage(dlg_hwnd, dlg_idlist, LB_ADDSTRING, 0, (LONG) buf);
}

void FillStatusBox(HWND hwnd, int id_list)
{
   UPSINFO *ups = getNextUps(NULL);

   dlg_hwnd = hwnd;
   dlg_idlist = id_list;
   output_status(ups, 0, stat_open, stat_list, stat_close);
}

#endif   /* HAVE_CYGWIN */
