#include <mc_control/Configuration.h>

#include <mc_rtc/logging.h>

#include <json/json.h>

#include <stdexcept>
#include <fstream>

namespace mc_control
{

struct Configuration::Json::Impl : public ::Json::Value
{
  using ::Json::Value::Value;
  Impl() : ::Json::Value() {}
  Impl(const ::Json::Value & v) : ::Json::Value(v) {}
};

bool Configuration::Json::isArray() const
{
  return impl->isArray();
}

size_t Configuration::Json::size() const
{
  return impl->size();
}

Configuration::Json Configuration::Json::operator[](int idx) const
{
  Configuration::Json ret;
  const ::Json::Value & v = (*impl)[idx];
  ret.impl.reset(new Impl(v));
  return ret;
}

Configuration::Json Configuration::Json::operator[](const std::string & key) const
{
  Configuration::Json ret;
  ret.impl.reset(new Impl((*impl)[key]));
  return ret;
}

Configuration::Exception::Exception(const std::string & msg)
: msg(msg)
{
}

const char * Configuration::Exception::what() const noexcept
{
  return msg.c_str();
}

Configuration::Configuration(const Json & v)
: v(v)
{
}

Configuration::Configuration(const char * path)
: Configuration(std::string(path))
{
}

bool Configuration::isMember(const std::string & key) const
{
  return v.impl->isMember(key);
}

Configuration Configuration::operator()(const std::string & key) const
{
  if(v.impl->isMember(key))
  {
    return Configuration(v[key]);
  }
  throw Configuration::Exception("No entry named " + key + " in the configuration");
}

Configuration::operator bool() const
{
  if(v.impl->isBool() || v.impl->isInt())
  {
    return v.impl->asBool();
  }
  throw Configuration::Exception("Stored Json value is not a bool");
}

Configuration::operator int() const
{
  if(v.impl->isInt())
  {
    return v.impl->asInt();
  }
  throw Configuration::Exception("Stored Json value is not an int");
}

Configuration::operator unsigned int() const
{
  if(v.impl->isUInt() || (v.impl->isInt() && v.impl->asInt() >= 0))
  {
    return v.impl->asUInt();
  }
  throw Configuration::Exception("Stored Json value is not an unsigned int");
}

Configuration::operator double() const
{
  if(v.impl->isNumeric())
  {
    return v.impl->asDouble();
  }
  throw Configuration::Exception("Stored Json value is not a double");
}

Configuration::operator std::string() const
{
  if(v.impl->isString())
  {
    return v.impl->asString();
  }
  throw Configuration::Exception("Stored Json v.impl->v.lue is not a string");
}

Configuration::operator Eigen::Vector3d() const
{
  if(v.isArray() && v.size() == 3 && v[0].impl->isNumeric())
  {
    Eigen::Vector3d ret;
    ret << v[0].impl->asDouble(), v[1].impl->asDouble(), v[2].impl->asDouble();
    return ret;
  }
  throw Configuration::Exception("Stored Json value is not a Vector3d");
}

Configuration::operator Eigen::Vector6d() const
{
  if(v.isArray() && v.size() == 6 && v[0].impl->isNumeric())
  {
    Eigen::Vector6d ret;
    ret << v[0].impl->asDouble(), v[1].impl->asDouble(), v[2].impl->asDouble(),
           v[3].impl->asDouble(), v[4].impl->asDouble(), v[5].impl->asDouble();
    return ret;
  }
  throw Configuration::Exception("Stored Json value is not a Vector6d");
}

Configuration::operator Eigen::VectorXd() const
{
  if(v.isArray() && (v.size() == 0 || v[0].impl->isNumeric()))
  {
    Eigen::VectorXd ret(v.size());
    for(size_t i = 0; i < v.size(); ++i)
    {
      ret(i) = v[static_cast<int>(i)].impl->asDouble();
    }
    return ret;
  }
  throw Configuration::Exception("Stored Json value is not a Vector6d");
}

Configuration::operator Eigen::Quaterniond() const
{
  if(v.isArray() && v.size() == 4 && v[0].impl->isNumeric())
  {
    return Eigen::Quaterniond(v[0].impl->asDouble(), v[1].impl->asDouble(),
                              v[2].impl->asDouble(), v[3].impl->asDouble())
                             .normalized();
  }
  throw Configuration::Exception("Stored Json value is not a Quaterniond");
}

Configuration::Configuration(const std::string & path)
: v()
{
  v.impl.reset(new Json::Impl());
  load(path);
}

void Configuration::load(const std::string & path)
{
  std::ifstream ifs(path);
  if(!ifs.is_open())
  {
    LOG_ERROR("Failed to open controller configuration file: " << path)
    return;
  }
  ::Json::Value newV;
  try
  {
    ifs >> newV;
  }
  catch(const std::exception & exc)
  {
    LOG_ERROR("Failed to read configuration file: " << path)
    LOG_WARNING(exc.what())
    return;
  }
  for(const auto & m : newV.getMemberNames())
  {
    (*v.impl)[m] = newV[m];
  }
}

template<>
void Configuration::operator()(const std::string & key, std::string & v) const
{
  try
  {
    v = (std::string)(*this)(key);
  }
  catch(Exception &)
  {
  }
}

}
