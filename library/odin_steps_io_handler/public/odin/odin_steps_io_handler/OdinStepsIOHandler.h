#ifndef ODINSTEPSIOHANDLER_H
#define ODINSTEPSIOHANDLER_H

#include "InetCommData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace odin
{
namespace io
{

enum class ERROR
{
    OK=0,
    OPEN_FAILED,
    WRITE_FAILED,
    READ_FAILED,
    BAD_HEADER,
    BAD_RECORD,
    RENAME_FAILED
};

struct Result
{
    ERROR error{ERROR::OK};
    std::string message;
    explicit operator bool() const { return error == ERROR::OK; }
};

class OdinStepsIOHandler
{
public:
    OdinStepsIOHandler(bool overwrite_file = true);
    ~OdinStepsIOHandler() = default;

    Result saveSteps(const std::filesystem::path & path,
                      const std::vector<OdinServoStep> & steps);

    Result loadSteps(const std::filesystem::path& path,
                  std::vector<OdinServoStep>& out,
                  bool strict=false);
    
    const char* toString(ERROR e);
private:
    void trim(std::string& s);
    bool validate(int servo, int pos, int spd, int dly);

    bool m_overwrite_file;

    static constexpr const char* m_steps_file_header = "# ODIN_STEPS v1";
};

} // io
} // odin


#endif // ODINSTEPSIOHANDLER_H
