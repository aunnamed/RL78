#pragma once
//=====================================================================//
/*!	@file
	@brief	ポート・ユーティリティー @n
			Copyright 2016 Kunihito Hiramatsu
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "G13/port.hpp"

namespace utils {

	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	/*!
		@brief  ポート・ユーティリティー・クラス
	*/
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	struct port {

		//-----------------------------------------------------------------//
		/*!
			@brief  全てのポートをプルアップ
		*/
		//-----------------------------------------------------------------//
		static void pullup_all()
		{
			device::PU0  = 0b1111'1111;
			device::PU1  = 0b1111'1111;
			device::PU3  = 0b1111'1111;
			device::PU4  = 0b1111'1111;
			device::PU5  = 0b1111'1111;
			device::PU6  = 0b1111'0000;
			device::PU7  = 0b1111'1111;
			device::PU8  = 0b1111'1111;
			device::PU9  = 0b1111'1111;
			device::PU10 = 0b0111'1111;
			device::PU11 = 0b1111'1111;
			device::PU12 = 0b1110'0001;
			device::PU14 = 0b1111'1111;
		}
	};
}