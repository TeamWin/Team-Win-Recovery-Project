// data.hpp - Base classes for data manager of GUI

#ifndef _DATA_HEADER
#define _DATA_HEADER

class DataManager
{
public:
    static int LoadValues(void);
    static int GetValue(const std::string varName, std::string& value);
    static int SetValue(const std::string varName, std::string value, int persist = 0);
    static int SetValue(const std::string varName, int value, int persist = 0);
    static int SetValue(const std::string varName, float value, int persist = 0);
    static int MapValue(const std::string varName, char* value);

protected:
    static std::map<std::string, std::string> mValues;
    static std::map<std::string, char*> mMappedValues;
};

#endif  // _DATA_HEADER

