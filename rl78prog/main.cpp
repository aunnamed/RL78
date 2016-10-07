//=====================================================================//
/*!	@file
	@brief	Renesas RL78 Series Programmer (Flash Writer)
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include <iostream>
#include "rl78_prog.hpp"
#include "conf_in.hpp"
#include "motsx_io.hpp"
#include "string_utils.hpp"
#include "area.hpp"

namespace {
	const std::string version_ = "0.10b";
	const std::string conf_file_ = "rl78_prog.conf";
	const uint32_t progress_num_ = 50;
	const char progress_cha_ = '#';

	utils::conf_in conf_in_;
	utils::motsx_io motsx_;
}

void memory_dump_()
{
#if 0
		int i = 0;
		for(auto v : mem) {
			std::cout << (boost::format("%02X, ") % static_cast<uint32_t>(v));
			++i;
			if(i >= 16) {
				i = 0;
				std::cout << std::endl;
			} 
		}
#endif
}

static const std::string get_current_path_(const std::string& exec)
{
	std::string exec_path;
#ifdef WIN32
	{
		auto tmp = utils::sjis_to_utf8(exec);
		exec_path = utils::convert_delimiter(tmp, '\\', '/');
	}
#else
	exec_path = exec;
#endif
	std::string spch;
	std::string env;
	{
#ifdef WIN32
		auto tmp = sjis_to_utf8(getenv("PATH"));
		env = utils::convert_delimiter(tmp, '\\', '/');
		spch = ';';
#else
		env = getenv("PATH");
		spch = ':';
#endif
	}
	utils::strings ss = utils::split_text(env, spch);
	for(const auto& s : ss) {
		std::string path = s + '/' + utils::get_file_name(exec_path);
		if(utils::probe_file(path)) {
			return s;
		}
	}

	return std::string("");
}


struct page_t {
	uint32_t	n = 0;
	uint32_t	c = 0;
};


static void progress_(uint32_t pageall, page_t& page)
{
	uint32_t pos = progress_num_ * page.n / pageall;
	for(uint32_t i = 0; i < (pos - page.c); ++i) {
		std::cout << progress_cha_ << std::flush;
	}
	page.c = pos;
}


struct options {
	bool verbose = false;

	std::string	inp_file;

	std::string	device;
	bool	dv = false;

	std::string	com_speed;
	bool	br = false;

	std::string com_path;
	std::string com_name;
	bool	dp = false;

	std::string voltage;
	bool	vt = false;

	utils::areas area_val;
	bool	area = false;

	bool	erase = false;
	bool	write = false;
	bool	verify = false;
	bool	device_list = false;
	bool	progress = false;
	bool	help = false;


	bool set_area_(const std::string& s) {
		utils::strings ss = utils::split_text(s, ",");
		std::string t;
		if(ss.empty()) t = s;
		else if(ss.size() >= 1) t = ss[0];
		uint32_t org = 0;
		bool err = false;
		if(!utils::string_to_hex(t, org)) {
			err = true;
		}
		uint32_t end = org + 256;
		if(ss.size() >= 2) {
			if(!utils::string_to_hex(ss[1], end)) {
				err = true;
			}
		}
		if(err) {
			return false;
		}
		area_val.emplace_back(org, end);
		return true;
	}


	bool set_str(const std::string& t) {
		bool ok = true;
		if(br) {
			com_speed = t;
			br = false;
		} else if(dv) {
			device = t;
			dv = false;
		} else if(dp) {
			com_path = t;
			dp = false;
		} else if(vt) {
			voltage = t;
			vt = false;
		} else if(area) {
			if(!set_area_(t)) {
				ok = false;
			}
			area = false;
		} else {
			inp_file = t;
		}
		return ok;
	}
};


static void help_(const std::string& cmd)
{
	using namespace std;

	std::string c = utils::get_file_base(cmd);

	cout << "Renesas RL78 Series Programmer Version " << version_ << endl;
	cout << "Copyright (C) 2016, Hiramatsu Kunihito (hira@rvf-rc45.net)" << endl;
	cout << "usage:" << endl;
	cout << c << " [options] [mot file] ..." << endl;
	cout << endl;
	cout << "Options :" << endl;
	cout << "    -P PORT,   --port=PORT        Specify serial port" << endl;
	cout << "    -s SPEED,  --speed=SPEED      Specify serial speed" << endl;
	cout << "    -d DEVICE, --device=DEVICE    Specify device name" << endl;
	cout << "    -V VOLTAGE, --voltage=VOLTAGE Specify CPU voltage" << endl;
	cout << "    -e, --erase                   Perform a device erase to a minimum" << endl;
	cout << "    -v, --verify                  Perform data verify" << endl;
	cout << "    -w, --write                   Perform data write" << endl;
	cout << "    --progress                    display Progress output" << endl;
	cout << "    --device-list                 Display device list" << endl;
	cout << "    --verbose                     Verbose output" << endl;
	cout << "    -h, --help                    Display this" << endl;
}


int main(int argc, char* argv[])
{
	if(argc == 1) {
		help_(argv[0]);
		return 0;
	}

	options	opts;

	// 設定ファイルの読み込み
	std::string conf_path;
	if(utils::probe_file(conf_file_)) {  // カレントにあるか？
		conf_path = conf_file_;
	} else {  // コマンド、カレントから読んでみる
		conf_path = get_current_path_(argv[0]) + '/' + conf_file_;
	}	
	if(conf_in_.load(conf_path)) {
		auto defa = conf_in_.get_default();
		opts.device = defa.device_;
		opts.com_path = defa.port_;
		opts.com_speed = defa.speed_;
		opts.voltage = defa.voltage_;
	} else {
		std::cerr << "Configuration file can't load: '" << conf_path << '\'' << std::endl;
		return -1;
	}

   	// コマンドラインの解析
	bool opterr = false;
	for(int i = 1; i < argc; ++i) {
		const std::string p = argv[i];
		if(p[0] == '-') {
			if(p == "--verbose") {
				opts.verbose = true;
			} else if(p == "-s") {
				opts.br = true;
			} else if(p.find("--speed=") == 0) {
				opts.com_speed = &p[std::strlen("--speed=")];
			} else if(p == "-d") {
				opts.dv = true;
			} else if(p.find("--device=") == 0) {
				opts.device = &p[std::strlen("--device=")];
			} else if(p == "-P") {
				opts.dp = true;
			} else if(p.find("--port=") == 0) {
				opts.com_path = &p[std::strlen("--port=")];
			} else if(p == "-V") {
				opts.vt = true;
			} else if(p.find("--voltage=") == 0) {
				opts.voltage = &p[std::strlen("--voltage=")];
///			} else if(p == "-a") {
///				opts.area = true;
///			} else if(p.find("--area=") == 0) {
///				if(!opts.set_area_(&p[7])) {
///					opterr = true;
///				}
			} else if(p == "-w" || p == "--write") {
				opts.write = true;
			} else if(p == "-v" || p == "--verify") {
				opts.verify = true;
			} else if(p == "--progress") {
				opts.progress = true;
			} else if(p == "--device-list") {
				opts.device_list = true;
			} else if(p == "-e" || p == "--erase") {
				opts.erase = true;
			} else if(p == "-h" || p == "--help") {
				opts.help = true;
			} else {
				opterr = true;
			}
		} else {
			if(!opts.set_str(p)) {
				opterr = true;
			}
		}
		if(opterr) {
			std::cerr << "Option error: '" << p << "'" << std::endl;
			opts.help = true;
		}
	}
	if(opts.verbose) {
		std::cout << "# Configuration file path: '" << conf_path << '\'' << std::endl;
		std::cout << "# Device: '" << opts.device << '\'' << std::endl;
		std::cout << "# Serial port path: '" << opts.com_path << '\'' << std::endl;
		std::cout << "# Serial port speed: " << opts.com_speed << std::endl;
	}

	// HELP 表示
	if(opts.help || (opts.inp_file.empty() && !opts.device_list) || opts.com_path.empty()
		|| opts.com_speed.empty() || opts.device.empty()) {
		help_(argv[0]);
		return 0;
	}

	// デバイス・リスト表示
	if(opts.device_list) {
		for(const auto& s : conf_in_.get_device_list()) {
			std::cout << s << std::endl;
		}
	}

	if(opts.inp_file.empty()) return 0;

	// 入力ファイルの読み込み
	uint32_t pageall = 0;
	if(!opts.inp_file.empty()) {
		if(opts.verbose) {
			std::cout << "Input file path: '" << opts.inp_file << '\'' << std::endl;
		}
		if(!motsx_.load(opts.inp_file)) {
			std::cerr << "Can't open input file: '" << opts.inp_file << "'" << std::endl;
			return -1;
		}
		pageall = motsx_.get_total_page();
		if(opts.verbose) {
			motsx_.list_area_map();
		}
	}

    // Windwos系シリアル・ポート（COMx）の変換
    if(!opts.com_path.empty() && opts.com_path[0] != '/') {
		std::string s = utils::to_lower_text(opts.com_path);
        if(s.size() > 3 && s[0] == 'c' && s[1] == 'o' && s[2] == 'm') {
            int val;
            if(utils::string_to_int(&s[3], val)) {
                if(val >= 1 ) {
                    --val;
                    opts.com_name = opts.com_path;
                    opts.com_path = "/dev/ttyS" + (boost::format("%d") % val).str();
                }
            }
        }
		if(opts.verbose) {
			std::cout << "Serial port alias: " << opts.com_name << " ---> " << opts.com_path << std::endl;
		}
    }
	if(opts.com_path.empty()) {
		std::cerr << "Serial port path not found." << std::endl;
		return -1;
	}
	if(opts.verbose) {
		std::cout << "Serial port path: '" << opts.com_path << '\'' << std::endl;
	}
	int com_speed = 0;
	if(!utils::string_to_int(opts.com_speed, com_speed)) {
		std::cerr << "Serial speed conversion error: '" << opts.com_speed << '\'' << std::endl;
		return -1;		
	}

	// CPU 電圧設定の変換
	int voltage = 0;
	if(!utils::string_to_int(opts.voltage, voltage)) {
		std::cerr << "CPU Operation voltage conversion error: '" << opts.voltage << '\'' << std::endl;
		return -1;		
	}
	if(voltage >= 16 && voltage <= 55) ;
	else {
		std::cerr << "CPU Operation voltage range error: "
			<< (voltage / 10) << '.' << (voltage % 10) << " [V]" << std::endl;
		return -1;		
	}

	rl78::prog prog_(opts.verbose);
	//=====================================
	if(!prog_.start(opts.com_path, com_speed, voltage)) {
		prog_.end();
		return -1;
	}

	// デバイスの確認
	//=====================================
	{
		const auto& sig = prog_.get_protocol().get_signature();
		char tmp[sizeof(sig.DEV) + 1];
		std::strcpy(tmp, reinterpret_cast<const char*>(sig.DEV));
		if(std::strncmp(opts.device.c_str(), tmp, std::strlen(tmp)) == 0) {
			std::cerr << "Device no match: '" << tmp << "'" << std::endl;
			prog_.end();
			return -1;
		}
	}

	if(opts.verbose) {
		const auto& sig = prog_.get_protocol().get_signature();
		sig.info("# ");
	}

	//=====================================
	if(opts.erase) {  // erase
		auto areas = motsx_.create_area_map();

		if(opts.progress) {
			std::cout << "Erase:  " << std::flush;
		}

		// ブロック：1024 バイト
		page_t page;
		for(const auto& a : areas) {
			uint32_t adr = a.min_ & 0xffffff00;
			uint32_t len = 0;
			uint32_t block = 0xffffffff;
			while(len < (a.max_ - a.min_ + 1)) {
				if(opts.progress) {
					progress_(pageall, page);
				}
				if((block & 0xfffffc00) != (adr & 0xfffffc00)) {
					if(!prog_.block_erase(adr)) {
						prog_.end();
						return -1;
					}
					block = adr;
				}
				adr += 256;
				len += 256;
				++page.n;
			}
		}
		if(opts.progress) {
			std::cout << std::endl << std::flush;
		}
	}

	//=====================================
	if(opts.write) {  // write
		auto areas = motsx_.create_area_map();
		if(opts.progress) {
			std::cout << "Write:  " << std::flush;
		}
		page_t page;
		for(const auto& a : areas) {
			uint32_t adr = a.min_ & 0xffffff00;
			uint32_t len = 0;
			uint32_t block = 0xffffffff;
			uint32_t block_len = 0;
			while(len < (a.max_ - a.min_ + 1)) {
				if(opts.progress) {
					progress_(pageall, page);
				}
				if((block & 0xfffffc00) != (adr & 0xfffffc00)) {
					uint32_t ln = a.max_ - adr + 1;
					if(ln > 1024) ln = 1024;
					else if(ln < 1024) { ln |= 0xff; ++ln; }
/// std::cout << boost::format("Start: %06X, %d") % adr % ln << std::endl << std::flush;
					if(!prog_.start_write(adr, adr + ln - 1)) {
						prog_.end();
						return -1;
					}
					block = adr;
					block_len = ln;
				}
				{
					auto mem = motsx_.get_memory(adr);
					bool last = false;
					if(block_len <= 256) last = true;
/// std::cout << boost::format("Write: %06X - %d") % adr % len << std::endl << std::flush;
					if(!prog_.write_page(&mem[0], 256, last)) {
						prog_.end();
						return -1;
					}
				}
				adr += 256;
				len += 256;
				block_len -= 256;
				++page.n;
			}
		}
		if(opts.progress) {
			std::cout << std::endl << std::flush;
		}
	}

	//=====================================
	if(opts.verify) {  // verify
		auto areas = motsx_.create_area_map();
		if(opts.progress) {
			std::cout << "Verify: " << std::flush;
		}
		page_t page;
		for(const auto& a : areas) {
			uint32_t adr = a.min_ & 0xffffff00;
			uint32_t len = 0;
			uint32_t block = 0xffffffff;
			uint32_t block_len = 0;
			while(len < (a.max_ - a.min_ + 1)) {
				if(opts.progress) {
					progress_(pageall, page);
				}
				if((block & 0xfffffc00) != (adr & 0xfffffc00)) {
					uint32_t ln = a.max_ - adr + 1;
					if(ln > 1024) ln = 1024;
					else if(ln < 1024) { ln |= 0xff; ++ln; }
/// std::cout << boost::format("Start: %06X, %d") % adr % ln << std::endl << std::flush;
					if(!prog_.start_verify(adr, adr + ln - 1)) {
						prog_.end();
						return -1;
					}
					block = adr;
					block_len = ln;
				}
				{
					auto mem = motsx_.get_memory(adr);
					bool last = false;
					if(block_len <= 256) last = true;
/// std::cout << boost::format("Write: %06X - %d") % adr % len << std::endl << std::flush;
					if(!prog_.verify_page(&mem[0], 256, last)) {
						prog_.end();
						return -1;
					}
				}
				adr += 256;
				len += 256;
				block_len -= 256;
				++page.n;
			}
		}
		if(opts.progress) {
			std::cout << std::endl << std::flush;
		}
	}

	prog_.end();
}
