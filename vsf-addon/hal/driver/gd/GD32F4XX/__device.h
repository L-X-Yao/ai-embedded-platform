/*****************************************************************************
 *   Copyright(C)2009-2022 by VSF Team                                       *
 *                                                                           *
 *  Licensed under the Apache License, Version 2.0 (the "License");          *
 *  you may not use this file except in compliance with the License.         *
 *  You may obtain a copy of the License at                                  *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 *  Unless required by applicable law or agreed to in writing, software      *
 *  distributed under the License is distributed on an "AS IS" BASIS,        *
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
 *  See the License for the specific language governing permissions and      *
 *  limitations under the License.                                           *
 *                                                                           *
 ****************************************************************************/

#if     defined(__GD32F405__)
#   define  VSF_DEVICE_HEADER       "./GD32F405/device.h"
#elif   defined(__GD32F407__)
#   define  VSF_DEVICE_HEADER       "./GD32F407/device.h"
#elif   defined(__GD32F425__)
#   define  VSF_DEVICE_HEADER       "./GD32F425/device.h"
#elif   defined(__GD32F427__)
#   define  VSF_DEVICE_HEADER       "./GD32F427/device.h"
#elif   defined(__GD32F450__)
#   define  VSF_DEVICE_HEADER       "./GD32F450/device.h"
#elif   defined(__GD32F470__)
#   define  VSF_DEVICE_HEADER       "./GD32F470/device.h"
#else
#   error No supported GD32F4XX device found.
#endif

#include VSF_DEVICE_HEADER
