#pragma once
#include <algorithm>
#include <chrono>
#include <clocale>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

std::wstring str2wstr(const std::string &s) {
    char *pcurLocale = ::setlocale(LC_ALL, NULL); // curLocale = "C";
    std::string curLocale;
    if (pcurLocale != nullptr) {
        curLocale.assign(pcurLocale);
    }

    const char *_Source = s.c_str();
    size_t _Dsize = s.size() + 1;
    wchar_t *_Dest = new wchar_t[_Dsize];
    wmemset(_Dest, 0, _Dsize);
    mbstowcs(_Dest, _Source, _Dsize);
    std::wstring result = _Dest;
    delete[] _Dest;

    ::setlocale(LC_ALL, curLocale.c_str());

    return result;
}

std::string wstr2str(const std::wstring &ws) {

    char *pcurLocale = ::setlocale(LC_ALL, NULL); // curLocale = "C";
    std::string curLocale;
    std::string result;
    if (pcurLocale != nullptr) {
        curLocale.assign(pcurLocale);
    }
    ::setlocale(LC_CTYPE, "chs");
    const wchar_t *_Source = ws.c_str();
    size_t _Dsize = 2 * ws.size() + 1;
    char *_Dest = new char[_Dsize];
    memset(_Dest, 0, _Dsize);
    size_t num = wcstombs(_Dest, _Source, _Dsize);
    if (num != (size_t)-1) {
        result.assign(_Dest, num);
    }
    delete[] _Dest;

    ::setlocale(LC_ALL, curLocale.c_str());

    return result;
}

struct ssp {
    uint32_t preSpace;
    template <class _Elem, class _Traits>
    typename std::basic_ostream<_Elem, _Traits> &
    operator()(typename std::basic_ostream<_Elem, _Traits> &_Ostr) {
        uint32_t sp = preSpace;
        while (sp--) {
            _Ostr.put(_Ostr.widen(' '));
        }
        return _Ostr;
    }
};

template <class _Elem, class _Traits>
auto operator<<(std::basic_ostream<_Elem, _Traits> &_Ostr, ssp &sps)
    -> std::basic_ostream<_Elem, _Traits> & {
    return sps(_Ostr);
}

// 友好显示文件大小
std::string FriendlyFileSize(uint64_t size) {
    std::stringstream output;
    double res = size;
    const char *const units[] = {"B", "KB", "MB", "GB", "TB"};
    int scale = 0;
    while (res > 1024.0 && scale < 4) {
        res /= 1024.0;
        scale++;
    }
    output << std::setprecision(3) << std::fixed << res << " " << units[scale];
    return output.str();
}

// 友好显示时间 (NTFS 时间(微软时间) 转 UTC 时间)
std::string NtfsTime(uint64_t time) {
    static char timeStr[50] = {};
    std::chrono::microseconds usTime{time / 10};
    std::chrono::system_clock::duration sys =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(usTime);
    std::chrono::time_point<std::chrono::system_clock> tp(sys);
    tp -= std::chrono::hours(24 * 365 * 369 + 89 * 24);
    std::time_t t_c = std::chrono::system_clock::to_time_t(tp);
    std::tm *ptm = std::localtime(&t_c);
    std::tm emptyTm = {0, 0, 0, 1, 0, -299};
    std::string ret;
    if (nullptr == ptm) ptm = &emptyTm;
    std::strftime(timeStr, 50, "UTC %F T%T %z", ptm);
    ret = timeStr;
    return ret;
}

// 展示 16 进制数据
void ShowHex(char const *data, uint64_t len, int width = 32, int preWhite = 0,
             uint32_t divisionHeight = (uint32_t)-1, bool showAscii = true) {
    uint64_t dpos = 0;
    uint32_t dDivide = divisionHeight;
    uint32_t blockNum = 1;
    while (dpos < len) {
        for (int p = preWhite; p; p--) {
            std::cout << " ";
        }
        for (int p = 0; p < width; p++) {
            if (dpos + p >= len) {
                for (int o = (width - p) * 3; o; o--) {
                    std::cout << " ";
                }
                break;
            }
            std::cout << std::uppercase << std::hex << std::setw(2)
                      << std::setfill('0') << (uint32_t)(uint8_t)data[dpos + p]
                      << " ";
        }
        if (showAscii) {
            std::cout << "    ";
            for (int p = 0; p < width; p++) {
                if (dpos + p >= len) {
                    break;
                }
                if ((uint8_t)data[dpos + p] >= 0x20 &&
                    (uint8_t)data[dpos + p] <= 0x7E) {
                    std::cout << data[dpos + p];
                }
                else {
                    std::cout << ".";
                }
            }
        }
        std::cout << std::endl;
        if (!(--dDivide)) {
            for (int p = preWhite; p; p--) {
                std::cout << " ";
            }
            std::cout << "  偏移: 0x" << std::hex << std::uppercase
                      << std::setw(8) << std::setfill('0') << dpos + width;
            std::cout << "  块号: " << std::dec << blockNum;
            std::cout << std::endl;
            dDivide = divisionHeight;
            blockNum++;
        }
        dpos += width;
    }
}

std::string toLowerCase(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::string toUpperCase(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

std::wstring toLowerCase(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(), ::towlower);
    return str;
}

std::wstring toUpperCase(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(), ::towupper);
    return str;
}

bool compareStrNoCase(std::string str1, std::string str2) {
    std::string cmp1 = toLowerCase(str1);
    std::string cmp2 = toLowerCase(str2);
    if (cmp1.compare(cmp2) == 0) {
        return true;
    }
    return false;
}

std::string trim(std::string str) {
    const char trim_char[] = {' ', '\t', '\r', '\n'};
    const unsigned short trim_char_count = 4;
    unsigned int begin = 0;
    int end = str.length() - 1;
    bool stop = false;
    if (str.length() > 0) {
        for (unsigned int p = 0; p < str.length(); p++) {
            for (int i = 0; i < trim_char_count; i++) {
                if (str.at(p) == trim_char[i]) {
                    begin = p + 1;
                    break;
                }
                if (i == trim_char_count - 1) {
                    stop = true;
                }
            }
            if (stop) {
                break;
            }
        }
        stop = false;
        for (int p = str.length() - 1; p >= 0; p--) {
            for (int i = 0; i < trim_char_count; i++) {
                if (str.at(p) == trim_char[i]) {
                    end = p - 1;
                    break;
                }
                if (i == trim_char_count - 1) {
                    stop = true;
                }
            }
            if (stop) {
                break;
            }
        }
    }
    return str.substr(begin, end - begin + 1);
}
