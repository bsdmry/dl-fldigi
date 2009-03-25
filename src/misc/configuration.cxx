#include <config.h>

#include "configuration.h"
#include "confdialog.h"
#include "xmlreader.h"
#include "soundconf.h"
#include "waterfall.h"
#include "main.h"
#include "gettext.h"

#if USE_HAMLIB
	#include "hamlib.h"
	#include "rigclass.h"
#endif

#include "rigMEM.h"
//#include "rigio.h"
#include "debug.h"

#include <FL/Fl_Tooltip.H>

#include <unistd.h>

#include <iostream>
#include <fstream>

#include <map>
#include <sstream>

#ifdef __linux__
#  include <dirent.h>
#  include <limits.h>
#  include <errno.h>
#endif
#ifdef __APPLE__
#  include <glob.h>
#endif
#ifndef __CYGWIN__
#  include <sys/stat.h>
#else
#  include <fcntl.h>
#endif

using namespace std;


const char *szBaudRates[] = {
	"", 
	"300","600","1200","2400",
	"4800","9600","19200","38400",
	"57600","115200","230400","460800"};
	
const char *szBands[] = {
	"",
	"1830", "3580", "7030", "7070", "10138",
	"14070", "18100", "21070", "21080", "24920", "28070", "28120", 0};


// Define stream I/O operators for non-builtin types.
// Right now we have: Fl_Color, Fl_Font, RGB, and RGBI
ostream& operator<<(ostream& out, const Fl_Color& c)
{
	return out << static_cast<int>(c);
}
istream& operator>>(istream& in, Fl_Color& c)
{
	int i;
	in >> i;
	c = static_cast<Fl_Color>(i);
	return in;
}
ostream& operator<<(ostream& out, const Fl_Font& f)
{
	return out << static_cast<int>(f);
}
istream& operator>>(istream& in, Fl_Font& f)
{
	int i;
	in >> i;
	f = static_cast<Fl_Font>(i);
	return in;
}
ostream& operator<<(ostream& out, const RGB& rgb)
{
	return out << (int)rgb.R << ' ' << (int)rgb.G << ' ' << (int)rgb.B;
}
istream& operator>>(istream& in, RGB& rgb)
{
	int i;
	in >> i; rgb.R = i;
	in >> i; rgb.G = i;
	in >> i; rgb.B = i;
	return 	in;

}
ostream& operator<<(ostream& out, const RGBI& rgbi)
{
	return out << (int)rgbi.R << ' ' << (int)rgbi.G << ' ' << (int)rgbi.B;
}
istream& operator>>(istream& in, RGBI& rgbi)
{
	int i;
	in >> i; rgbi.R = i;
	in >> i; rgbi.G = i;
	in >> i; rgbi.B = i;
	return 	in;
}

// This allows to put tag elements into containers
class tag_base
{
public:
	tag_base(const char* t) : tag(t) { }
	virtual void write(ostream& out) const = 0;
	virtual void read(const char* data) = 0;
	virtual ~tag_base() { }
	const char* tag;
};

// This will handle every type that has << and >> stream operators
template <typename T>
class tag_elem : public tag_base
{
public:
	tag_elem(const char* t, T& v) : tag_base(t), var(v) { }
	void write(ostream& out) const
        {
		out << '<' << tag << '>' << var << "</" << tag << ">\n";
	}
	void read(const char* data)
	{
		istringstream iss(data);
		iss >> var;
	}
	T& var;
};

// Instantiate an explicit tag_elem<T> for types that require unusual handling.

// Special handling for strings
template <>
class tag_elem<string> : public tag_base
{
public:
	tag_elem(const char* t, string& s) : tag_base(t), str(s) { }
	void write(ostream& out) const
        {
		string s = str;
		string::size_type i = s.find('&');

		while (i != string::npos) {
			s.replace(i, 1, "&amp;");
			i = s.find('&', i + 1);
		}
		while ((i = s.find('<')) != string::npos)
			s.replace(i, 1, "&lt;");
		while ((i = s.find('>')) != string::npos)
			s.replace(i, 1, "&gt;");
		while ((i = s.find('"')) != string::npos)
			s.replace(i, 1, "&quot;");
		while ((i = s.find('\'')) != string::npos)
			s.replace(i, 1, "&apos;");

		out << '<' << tag << '>' << s << "</" << tag << ">\n";
	}
	void read(const char* data) { str = data; }
	string& str;
};


// By redefining the ELEM_ macro, we can control what the CONFIG_LIST macro
// will expand to, and accomplish several things:
// 1) Declare "struct configuration". See ELEM_DECLARE_CONFIGURATION
//    in configuration.h.
// 2) Define progdefaults, the configuration struct that is initialised with
//    fldigi's default options
#define ELEM_PROGDEFAULTS(type_, var_, tag_, ...) __VA_ARGS__,
// 3) Define an array of tag element pointers
#define ELEM_TAG_ARRAY(type_, var_, tag_, ...) (*tag_ ? new tag_elem<type_>(tag_, progdefaults.var_) : 0),

// First define the default config
#undef ELEM_
#define ELEM_ ELEM_PROGDEFAULTS
configuration progdefaults = { CONFIG_LIST };


void configuration::writeDefaultsXML()
{
	string deffname(HomeDir);
	deffname.append("fldigi_def.xml");

	string deffname_backup(deffname);
	deffname_backup.append("-old");
	rename(deffname.c_str(), deffname_backup.c_str());

	ofstream f(deffname.c_str());
	if (!f) {
		LOG_ERROR("Could not write %s", deffname.c_str());
		return;
	}

	// create an array
#undef ELEM_
#define ELEM_ ELEM_TAG_ARRAY
	tag_base* tag_list[] = { CONFIG_LIST };

	// write all variables with non-empty tags to f
	f << "<FLDIGI_DEFS>\n";
	for (size_t i = 0; i < sizeof(tag_list)/sizeof(*tag_list); i++) {
		if (tag_list[i]) {
			tag_list[i]->write(f);
			delete tag_list[i];
		}
	}
	f << "</FLDIGI_DEFS>\n";
	f.close();
}

bool configuration::readDefaultsXML()
{
	string deffname = HomeDir;
	deffname.append("fldigi_def.xml");
	ifstream f(deffname.c_str());
	if (!f)
		return false;

	string xmlbuf;
	f.seekg(0, ios::end);
	xmlbuf.reserve(f.tellg()); // reserve some space to avoid reallocations
	f.seekg(0, ios::beg);

	char line[BUFSIZ];
	while (f.getline(line, sizeof(line)))
		xmlbuf.append(line).append("\n");
	f.close();

	IrrXMLReader* xml = createIrrXMLReader(new IIrrXMLStringReader(xmlbuf));
	if (!xml)
		return false;

	// create a TAG_NAME -> ELEMENT map
	typedef map<string, tag_base*> tag_map_t;
	tag_map_t tag_map;

	tag_base* tag_list[] = { CONFIG_LIST };
	for (size_t i = 0; i < sizeof(tag_list)/sizeof(*tag_list); i++)
		if (tag_list[i])
			tag_map[tag_list[i]->tag] = tag_list[i];

	// parse the xml buffer
	tag_map_t::const_iterator i;
	while(xml->read()) {
		switch(xml->getNodeType()) {
		case EXN_TEXT:
		case EXN_CDATA:
			if (i != tag_map.end()) // do we know about this tag?
				i->second->read(xml->getNodeData());
			break;
		case EXN_ELEMENT_END:
			i = tag_map.end(); // ignore the next EXN_CDATA
			break;
		case EXN_ELEMENT:
			i = tag_map.find(xml->getNodeName());
			break;
		case EXN_NONE: case EXN_COMMENT: case EXN_UNKNOWN:
			break;
		}
	}

	delete xml;
	// delete the tag objects
	for (size_t i = 0; i < sizeof(tag_list)/sizeof(*tag_list); i++)
		delete tag_list[i];

	return true;
}

void configuration::loadDefaults()
{
// RTTY
	selShift->value(rtty_shift);
	selBaud->value(rtty_baud);
	selBits->value(rtty_bits);
	selParity->value(rtty_parity);
//	chkMsbFirst->value(rtty_msbfirst);
	selStopBits->value(rtty_stop);
	btnCRCRLF->value(rtty_crcrlf);
	btnAUTOCRLF->value(rtty_autocrlf);
	cntrAUTOCRLF->value(rtty_autocount);
	chkPseudoFSK->value(PseudoFSK);
	chkUOSrx->value(UOSrx);
	chkUOStx->value(UOStx);
	chkXagc->value(Xagc);

	mnuRTTYAFCSpeed->value(rtty_afcspeed);
	btnPreferXhairScope->value(PreferXhairScope);

// OLIVIA
	mnuOlivia_Tones->value(oliviatones);
	mnuOlivia_Bandwidth->value(oliviabw);
	cntOlivia_smargin->value(oliviasmargin);
	cntOlivia_sinteg->value(oliviasinteg);
	btnOlivia_8bit->value(olivia8bit);

	chkDominoEX_FEC->value(DOMINOEX_FEC);

	btnmt63_interleave->value(mt63_interleave == 64);

	Fl_Tooltip::enable(tooltips);
}

void configuration::saveDefaults()
{
	ENSURE_THREAD(FLMAIN_TID);

	memcpy(&cfgpal0, &palette[0], sizeof(cfgpal0));
	memcpy(&cfgpal1, &palette[1], sizeof(cfgpal1));
	memcpy(&cfgpal2, &palette[2], sizeof(cfgpal2));
	memcpy(&cfgpal3, &palette[3], sizeof(cfgpal3));
	memcpy(&cfgpal4, &palette[4], sizeof(cfgpal4));
	memcpy(&cfgpal5, &palette[5], sizeof(cfgpal5));
	memcpy(&cfgpal6, &palette[6], sizeof(cfgpal6));
	memcpy(&cfgpal7, &palette[7], sizeof(cfgpal7));
	memcpy(&cfgpal8, &palette[8], sizeof(cfgpal8));

	RxFontName = Fl::get_font_name(RxFontnbr);
	TxFontName = Fl::get_font_name(TxFontnbr);
	WaterfallFontName = Fl::get_font_name(WaterfallFontnbr);
	ViewerFontName = Fl::get_font_name(ViewerFontnbr);

	writeDefaultsXML();
	changed = false;
}

#if USE_HAMLIB
static int fill_hamlib_menu(const char* rigname)
{
	cboHamlibRig->add(rigname);
	return 1;
}
#endif

int configuration::setDefaults()
{
	ENSURE_THREAD(FLMAIN_TID);

#if USE_HAMLIB
	hamlib_get_rigs();
	hamlib_get_rig_str(fill_hamlib_menu);
	if (HamRigModel == 0 && !HamRigName.empty()) { // compatibility with < 3.04
		HamRigModel = hamlib_get_rig_model_compat(HamRigName.c_str());
		LOG_INFO("Found rig model %d for \"%s\"", HamRigModel, HamRigName.c_str());
	}
#endif

	inpMyCallsign->value(myCall.c_str());
	inpMyName->value(myName.c_str());
	inpMyQth->value(myQth.c_str());
	inpMyLocator->value(myLocator.c_str());
	inpMyAntenna->value(myAntenna.c_str());
	UseLeadingZeros = btnUseLeadingZeros->value();
	ContestStart = (int)nbrContestStart->value();
	ContestDigits = (int)nbrContestDigits->value();

	txtSecondary->value(secText.c_str());

	txtTHORSecondary->value(THORsecText.c_str());
	valTHOR_BW->value(THOR_BW);
	valTHOR_FILTER->value(THOR_FILTER);
	valTHOR_PATHS->value(THOR_PATHS);
	valTHOR_SOFT->value(THOR_SOFT);
	valThorCWI->value(ThorCWI);

	valDominoEX_BW->value(DOMINOEX_BW);
	valDominoEX_FILTER->value(DOMINOEX_FILTER);
	chkDominoEX_FEC->value(DOMINOEX_FEC);
	valDominoEX_PATHS->value(DOMINOEX_PATHS);
	valDomCWI->value(DomCWI);

    btnMEMMAPptt->value(MEMMAPptt);
    btnRigCatCMDptt->value(RigCatCMDptt);
    btnTTYptt->value(TTYptt);
    
#if USE_HAMLIB
	mnuSideband->add("Rig mode");
	mnuSideband->add("Always LSB");
	mnuSideband->add("Always USB");
	mnuSideband->value(HamlibSideband);
    btnHamlibCMDptt->value(HamlibCMDptt);
    inpRIGdev->show();
	mnuBaudRate->show();
	cboHamlibRig->show();
	cboHamlibRig->value(HamRigName.c_str());
#else
	tabHamlib->parent()->remove(*tabHamlib);
#endif
	btnRTSptt->value(RTSptt);
	btnDTRptt->value(DTRptt);
	btnRTSplusV->value(RTSplus);
	btnDTRplusV->value(DTRplus);

	inpTTYdev->value(PTTdev.c_str());

	if(chkUSEMEMMAPis) {
		chkUSEMEMMAP->value(1); 
		chkUSEHAMLIB->value(0); chkUSERIGCAT->value(0); chkUSEXMLRPC->value(0);
	} else if (chkUSEHAMLIBis) {
		chkUSEHAMLIB->value(1);
		chkUSEMEMMAP->value(0); chkUSERIGCAT->value(0);  chkUSEXMLRPC->value(0);
	} else if (chkUSERIGCATis) {
		chkUSERIGCAT->value(1);
		chkUSEMEMMAP->value(0); chkUSEHAMLIB->value(0); chkUSEXMLRPC->value(0);
	} else if (chkUSEXMLRPCis) {
		chkUSEXMLRPC->value(1);
		chkUSEMEMMAP->value(0); chkUSEHAMLIB->value(0); chkUSERIGCAT->value(0);
	} else {
		chkUSEMEMMAP->value(0); chkUSEHAMLIB->value(0); 
		chkUSERIGCAT->value(0);	chkUSEHAMLIB->value(0); chkUSEXMLRPC->value(0);
	}

	inpRIGdev->value(HamRigDevice.c_str());
	mnuBaudRate->value(HamRigBaudrate);

#if !USE_XMLRPC
	tabXMLRPC->parent()->remove(*tabXMLRPC);
#endif

	inpXmlRigDevice->value(XmlRigDevice.c_str());
	mnuXmlRigBaudrate->value(XmlRigBaudrate);

	valCWsweetspot->value(CWsweetspot);
	valRTTYsweetspot->value(RTTYsweetspot);
	valPSKsweetspot->value(PSKsweetspot);
	btnWaterfallHistoryDefault->value(WaterfallHistoryDefault);
	btnWaterfallQSY->value(WaterfallQSY);

	inpWaterfallClickText->input_type(FL_MULTILINE_INPUT);
	inpWaterfallClickText->value(WaterfallClickText.c_str());
	if (!WaterfallClickInsert)
		inpWaterfallClickText->deactivate();

	mnuWaterfallWheelAction->add(waterfall::wf_wheel_action);
	mnuWaterfallWheelAction->value(WaterfallWheelAction);

	btnStartAtSweetSpot->value(StartAtSweetSpot);
	btnPSKmailSweetSpot->value(PSKmailSweetSpot);
	cntSearchRange->value(SearchRange);
	cntServerOffset->value(ServerOffset);
	cntACQsn->value(ACQsn);
			
	btnCursorBWcolor->color(
		fl_rgb_color(cursorLineRGBI.R, cursorLineRGBI.G, cursorLineRGBI.B) );
	btnCursorCenterLineColor->color(
		fl_rgb_color(cursorCenterRGBI.R, cursorCenterRGBI.G, cursorCenterRGBI.B) );
	btnBwTracksColor->color(
		fl_rgb_color(bwTrackRGBI.R, bwTrackRGBI.G, bwTrackRGBI.B) );
				
	cntCWweight->value(CWweight);
	sldrCWxmtWPM->value(CWspeed);
	cntCWdefWPM->value(defCWspeed);
	sldrCWbandwidth->value(CWbandwidth);
	btnCWrcvTrack->value(CWtrack);
	cntCWrange->value(CWrange);
	cntCWlowerlimit->value(CWlowerlimit);
	cntCWupperlimit->value(CWupperlimit);
	cntCWlowerlimit->maximum(CWupperlimit - 20);
	cntCWupperlimit->minimum(CWlowerlimit + 20);
	cntCWrisetime->value(CWrisetime);
	cntCWdash2dot->value(CWdash2dot);
	mnuQSKshape->value(QSKshape);
	sldrCWxmtWPM->minimum(CWlowerlimit);
	sldrCWxmtWPM->maximum(CWupperlimit);
	btnQSK->value(QSK);
	cntPreTiming->value(CWpre);
	cntPostTiming->value(CWpost);
	btnCWID->value(CWid);
			
	selHellFont->value(feldfontnbr);
	btnFeldHellIdle->value(HellXmtIdle);
			
	chkTransmitRSid->value(TransmitRSid);
	chkRSidWideSearch->value(rsidWideSearch);
	chkSlowCpu->value(slowcpu);
	
	Fl_Button* qrzb = btnQRZnotavailable;
	switch (QRZ) {
	case QRZNONE:
		qrzb = btnQRZnotavailable;
		break;
	case QRZCD:
		qrzb = btnQRZcdrom;
		break;
	case QRZHTML:
		qrzb = btnQRZonline;
		break;
	case QRZNET:
		qrzb = btnQRZsub;
		break;
	case HAMCALLNET:
		qrzb = btnHamcall;
		break;
	case HAMCALLHTML:
		qrzb = btnHAMCALLonline;
		break;
	default :
		break;
	}
	set_qrz_buttons(qrzb);
	txtQRZpathname->value(QRZpathname.c_str());

	btnsendid->value(sendid);
	btnsendvideotext->value(sendtextid);
	chkID_SMALL->value(ID_SMALL);

	wf->setPrefilter(wfPreFilter);
	valLatency->value(latency);
	btnWFaveraging->value(WFaveraging);

	memcpy(&palette[0], &cfgpal0, sizeof(palette[0]));
	memcpy(&palette[1], &cfgpal1, sizeof(palette[1]));
	memcpy(&palette[2], &cfgpal2, sizeof(palette[2]));
	memcpy(&palette[3], &cfgpal3, sizeof(palette[3]));
	memcpy(&palette[4], &cfgpal4, sizeof(palette[4]));
	memcpy(&palette[5], &cfgpal5, sizeof(palette[5]));
	memcpy(&palette[6], &cfgpal6, sizeof(palette[6]));
	memcpy(&palette[7], &cfgpal7, sizeof(palette[7]));
	memcpy(&palette[8], &cfgpal8, sizeof(palette[8]));

	wf->setcolors();
	setColorButtons();
	
#if HAVE_UHROUTER
    btnUseUHrouterPTT->show();
#else
    btnUseUHrouterPTT->hide();
#endif

#if HAVE_PARPORT
    btnUsePPortPTT->show();
#else
    btnUsePPortPTT->hide();
#endif

	return 1;
}

void configuration::resetDefaults(void)
{
	if (!fl_choice(_("Reset all options to their default values?"), "OK", "Cancel", NULL) &&
	    Fl::event_key() != FL_Escape) {
		fl_message(_("Reset options will take effect at the next start"));
		reset();
		atexit(reset);
	}
}

void configuration::reset(void)
{
	remove(string(HomeDir).append("fldigi_def.xml").c_str());
	remove(string(HomeDir).append("fldigi.prefs").c_str());
}

#include "rigio.h"

void configuration::initInterface()
{
	ENSURE_THREAD(FLMAIN_TID);

// close down any possible rig interface threads
#if USE_HAMLIB
	hamlib_close();
//		MilliSleep(100);
#endif
	rigMEM_close();
//		MilliSleep(100);
	rigCAT_close();
//		MilliSleep(100);

    MEMMAPptt = btnMEMMAPptt->value();
    RigCatCMDptt = btnRigCatCMDptt->value();
    TTYptt = btnTTYptt->value();

	RTSptt = btnRTSptt->value();
	DTRptt = btnDTRptt->value();
	RTSplus = btnRTSplusV->value();
	DTRplus = btnDTRplusV->value();

	PTTdev = inpTTYdev->value();

#if USE_HAMLIB
	chkUSEHAMLIBis = chkUSEHAMLIB->value();
     HamlibCMDptt = btnHamlibCMDptt->value();
#endif
	chkUSEMEMMAPis = chkUSEMEMMAP->value();
	chkUSERIGCATis = chkUSERIGCAT->value();

#if USE_HAMLIB
	if (*cboHamlibRig->value() == '\0') // no selection at start up
		cboHamlibRig->index(hamlib_get_index(HamRigModel));
	else
		HamRigModel = hamlib_get_rig_model(cboHamlibRig->index());
	HamRigDevice = inpRIGdev->value();
	HamRigBaudrate = mnuBaudRate->value();
#else
	cboHamlibRig->hide();
	inpRIGdev->hide();
	mnuBaudRate->hide();
#endif

	bool riginitOK = false;

	if (chkUSEMEMMAPis) {// start the memory mapped i/o thread
		if (rigMEM_init()) {
			wf->setQSY(1);
			if (docked_rig_control)
				qsoFreqDisp->activate();
			riginitOK = true;
		}
	} else if (chkUSERIGCATis) { // start the rigCAT thread
		if (rigCAT_init(true)) {
			wf->USB(true);
			wf->setQSY(1);
			if (docked_rig_control)
				qsoFreqDisp->activate();
			riginitOK = true;
		}
#if USE_HAMLIB
	} else if (chkUSEHAMLIBis) { // start the hamlib thread
		if (hamlib_init(HamlibCMDptt)) {
			wf->USB(true);
			wf->setQSY(1);
			if (docked_rig_control)
				qsoFreqDisp->activate();
			riginitOK = true;
		}
#endif
	} else if (chkUSEXMLRPCis) {
		wf->setXMLRPC(1);
	}

	if (riginitOK == false) {
		rigCAT_init(false);
		wf->USB(true);
		wf->setQSY(0);
		if (docked_rig_control)
			qsoFreqDisp->activate();
	}

    if (HamlibCMDptt && chkUSEHAMLIBis) 
        push2talk->reset(PTT::PTT_HAMLIB);
    else if (MEMMAPptt && chkUSEMEMMAPis) 
        push2talk->reset(PTT::PTT_MEMMAP);
    else if ((RigCatCMDptt || RigCatRTSptt || RigCatDTRptt) && chkUSERIGCATis)
        push2talk->reset(PTT::PTT_RIGCAT);
    else if (TTYptt)
    	push2talk->reset(PTT::PTT_TTY);
    else
        push2talk->reset(PTT::PTT_NONE);
    	
	wf->setRefLevel();
	wf->setAmpSpan();
	cntLowFreqCutoff->value(LowFreqCutoff);

}

const char* configuration::strBaudRate()
{
	return (szBaudRates[HamRigBaudrate + 1]);
}

int configuration::BaudRate(size_t n)
{
	if (n > sizeof(szBaudRates) + 1) return 1200;
	return (atoi(szBaudRates[n + 1]));
}

void configuration::testCommPorts()
{
	inpTTYdev->clear();
	inpRIGdev->clear();
	inpXmlRigDevice->clear();
	
#ifndef PATH_MAX
#  define PATH_MAX 1024
#endif
#ifndef __CYGWIN__
	struct stat st;
#endif
	char ttyname[PATH_MAX + 1];

#ifdef __linux__
	bool ret = false;
	DIR* sys = NULL;
	char cwd[PATH_MAX] = { '.', '\0' };
	if (getcwd(cwd, sizeof(cwd)) == NULL || chdir("/sys/class/tty") == -1 ||
	    (sys = opendir(".")) == NULL)
		goto out;

	ssize_t len;
	struct dirent* dp;
	while ((dp = readdir(sys))) {
#  ifdef _DIRENT_HAVE_D_TYPE
		if (dp->d_type != DT_LNK)
			continue;
#  endif
		if ((len = readlink(dp->d_name, ttyname, sizeof(ttyname)-1)) == -1)
			continue;
		ttyname[len] = '\0';
		if (!strstr(ttyname, "/devices/virtual/")) {
			snprintf(ttyname, sizeof(ttyname), "/dev/%s", dp->d_name);
			if (stat(ttyname, &st) == -1 || !S_ISCHR(st.st_mode))
				continue;
			LOG_INFO("Found serial port %s", ttyname);
			inpTTYdev->add(ttyname);
#if USE_HAMLIB
			inpRIGdev->add(ttyname);
#endif
			inpXmlRigDevice->add(ttyname);
		}
	}
	ret = true;

out:
	if (sys)
		closedir(sys);
	chdir(cwd);
	if (ret) // do we need to fall back to the probe code below?
		return;
#endif // __linux__


	const char* tty_fmt[] = {
#if defined(__linux__)
		"/dev/ttyS%u",
		"/dev/ttyUSB%u",
		"/dev/usb/ttyUSB%u"
#elif defined(__FreeBSD__)
		"/dev/ttyd%u"
#elif defined(__CYGWIN__)
		"/dev/ttyS%u"
#elif defined(__APPLE__)
		"/dev/cu.*",
		"/dev/tty.*"
#endif
	};

#if defined(__CYGWIN__)
	int fd;
#  define TTY_MAX 255
#elif defined(__APPLE__)
	glob_t gbuf;
#else
#  define TTY_MAX 8
#endif

	for (size_t i = 0; i < sizeof(tty_fmt)/sizeof(*tty_fmt); i++) {
#ifndef __APPLE__
		for (unsigned j = 0; j < TTY_MAX; j++) {
			snprintf(ttyname, sizeof(ttyname), tty_fmt[i], j);
#  ifndef __CYGWIN__
			if ( !(stat(ttyname, &st) == 0 && S_ISCHR(st.st_mode)) )
				continue;
#  else // __CYGWIN__
			if ((fd = open(ttyname, O_RDWR | O_NOCTTY | O_NDELAY)) == -1)
				continue;
			snprintf(ttyname, sizeof(ttyname), "COM%u", j+1);
			close(fd);
#  endif // __CYGWIN__

			LOG_INFO("Found serial port %s", ttyname);
			inpTTYdev->add(ttyname);
#  if USE_HAMLIB
			inpRIGdev->add(ttyname);
#  endif
			inpXmlRigDevice->add(ttyname);
		}
#else // __APPLE__
		glob(tty_fmt[i], 0, NULL, &gbuf);
		for (size_t j = 0; j < gbuf.gl_pathc; j++) {
			if ( !(stat(gbuf.gl_pathv[j], &st) == 0 && S_ISCHR(st.st_mode)) ||
			     strstr(gbuf.gl_pathv[j], "modem") )
				continue;
			LOG_INFO("Found serial port %s", gbuf.gl_pathv[j]);
			inpTTYdev->add(gbuf.gl_pathv[j]);
#  if USE_HAMLIB
			inpRIGdev->add(gbuf.gl_pathv[j]);
#  endif
			inpXmlRigDevice->add(ttyname);

		}
		globfree(&gbuf);
#endif // __APPLE__
	}

#if HAVE_UHROUTER
	if (stat(UHROUTER_FIFO_PREFIX "Read", &st) != -1 && S_ISFIFO(st.st_mode) &&
	    stat(UHROUTER_FIFO_PREFIX "Write", &st) != -1 && S_ISFIFO(st.st_mode))
		inpTTYdev->add(UHROUTER_FIFO_PREFIX);
#endif // HAVE_UHROUTER
}

Fl_Font font_number(const char* name)
{
    int n = (int)Fl::set_fonts(0);
    for (int i = 0; i < n; i++) {
        if (strcmp(Fl::get_font_name((Fl_Font)i), name) == 0)
            return (Fl_Font)i;
    }
	return FL_HELVETICA;
}

void configuration::initFonts(void)
{
	if (!RxFontName.empty())
		RxFontnbr = font_number(RxFontName.c_str());
	if (!TxFontName.empty())
		TxFontnbr = font_number(TxFontName.c_str());
	if (!WaterfallFontName.empty())
		WaterfallFontnbr = font_number(WaterfallFontName.c_str());
	if (!ViewerFontName.empty())
		ViewerFontnbr = font_number(ViewerFontName.c_str());

}
