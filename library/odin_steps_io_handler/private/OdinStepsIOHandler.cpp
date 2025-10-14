#include "odin/odin_steps_io_handler/OdinStepsIOHandler.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace odin
{
namespace io
{

OdinStepsIOHandler::OdinStepsIOHandler(bool overwrite_file):
    m_overwrite_file(overwrite_file){}

const char* OdinStepsIOHandler::toString(ERROR e)
{
    switch (e) {
        case ERROR::OK:            return "Ok";
        case ERROR::OPEN_FAILED:   return "Open failed";
        case ERROR::WRITE_FAILED:  return "Write failed";
        case ERROR::READ_FAILED:   return "Read failed";
        case ERROR::BAD_HEADER:    return "Bad header";
        case ERROR::BAD_RECORD:    return "Bad record";
        case ERROR::RENAME_FAILED: return "Rename failed";
        default:                   return "Unknown";
    }
}

void OdinStepsIOHandler::trim(std::string & str)
{
    auto is_not_space = [](unsigned char ch){ return !std::isspace(ch); };
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), is_not_space));
    str.erase(std::find_if(str.rbegin(), str.rend(), is_not_space).base(), str.end());
}

bool OdinStepsIOHandler::validate(int servo_num, int position, int speed, int delay){
    return (servo_num>=1 && servo_num<=6) &&
           (position>=500 && position<=2500) &&
           (speed>=1 && speed<=10) &&
           (delay>=0);
}

Result OdinStepsIOHandler::saveSteps(const std::filesystem::path & path,
                                     const std::vector<OdinServoStep> & steps)
{
    std::error_code ec;
    const auto tmp = std::filesystem::path(path.string() + ".tmp");

    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) return {ERROR::OPEN_FAILED, "Cannot open tmp file: " + tmp.string()};

    ofs << m_steps_file_header << '\n';
    for (const auto& s : steps) {
        ofs << int(s.servo_num) << ' '
            << int(s.position)  << ' '
            << int(s.speed)     << ' '
            << int(s.delay)     << '\n';
    }
    ofs << '\n';
    ofs.flush();
    if (!ofs.good())
    {
        ofs.close();
        std::remove(tmp.string().c_str());
        return { ERROR::WRITE_FAILED, "Write failed: " + tmp.string() };
    }
    ofs.close();

    if (std::filesystem::exists(path, ec)) std::filesystem::remove(path, ec);

    std::filesystem::rename(tmp, path, ec);
    if (ec)
    {
        std::remove(tmp.string().c_str());
        return {ERROR::RENAME_FAILED, "Rename failed: " + ec.message()};
    }
    return {};
}

Result OdinStepsIOHandler::loadSteps(const std::filesystem::path & path,
                                     std::vector<OdinServoStep> & out,
                                     bool strict)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return {ERROR::OPEN_FAILED, "Cannot open: " + path.string()};

    std::string line;
    if (!std::getline(ifs, line))
        return {ERROR::READ_FAILED, "Empty file"};

    trim(line);
    if (line != m_steps_file_header)
        return {ERROR::BAD_HEADER, "Expected header '" + std::string(m_steps_file_header) + "', got '" + line + "'"};

    out.clear();
    std::size_t lineno = 1;

    while (std::getline(ifs, line))
    {
        ++lineno;
        trim(line);
        if (line.empty() || line[0]=='#') continue;

        std::istringstream iss(line);
        int servo, pos, spd, dly;
        if (!(iss >> servo >> pos >> spd >> dly))
        {
            if (strict) return {ERROR::BAD_RECORD, "Parse error at line " + std::to_string(lineno)};
            continue;
        }
        if (!validate(servo, pos, spd, dly))
        {
            if (strict) return {ERROR::BAD_RECORD, "Validation failed at line " + std::to_string(lineno)};
            continue;
        }

        OdinServoStep s{};
        s.step_num  = static_cast<std::uint64_t>(out.size());
        s.servo_num = static_cast<std::uint8_t>(servo);
        s.position  = static_cast<std::uint16_t>(pos);
        s.speed     = static_cast<std::uint8_t>(spd);
        s.delay     = static_cast<std::uint64_t>(dly);
        out.push_back(s);
    }

    if (ifs.bad()) return {ERROR::READ_FAILED, "I/O error while reading"};
    return {};
}

} // io
} // odin