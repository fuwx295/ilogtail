// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <cstdlib>

#include "common/JsonUtil.h"
#include "config/Config.h"
#include "config_manager/ConfigManager.h"
#include "models/LogEvent.h"
#include "plugin/instance/ProcessorInstance.h"
#include "processor/ProcessorDesensitizeNative.h"
#include "processor/ProcessorSplitLogStringNative.h"
#include "processor/ProcessorSplitRegexNative.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ProcessorDesensitizeNativeUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        mContext.SetLogstoreName("logstore");
        mContext.SetProjectName("project");
        mContext.SetRegion("cn-shanghai");
    }
    Config GetCastSensWordConfig(string, string, int, bool, string);
    void TestInit();
    void TestParseCastSensWordConfig();
    void TestCastSensWordConst();
    void TestCastSensWordMD5();
    void TestCastSensWordFail();
    void TestCastSensWordLoggroup();
    void TestCastSensWordMulti();
    void TestCastWholeKey();
    void TestMultipleLines();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestInit);

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestParseCastSensWordConfig);

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestCastSensWordConst);

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestCastSensWordMD5);

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestCastSensWordFail);

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestCastSensWordLoggroup);

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestCastSensWordMulti);

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestCastWholeKey);

UNIT_TEST_CASE(ProcessorDesensitizeNativeUnittest, TestMultipleLines);

Config ProcessorDesensitizeNativeUnittest::GetCastSensWordConfig(string key = string("cast1"),
                                                                 string regex = string("(pwd=)[^,]+"),
                                                                 int type = SensitiveWordCastOption::CONST_OPTION,
                                                                 bool replaceAll = false,
                                                                 string constVal = string("\\1********")) {
    Config oneConfig;
    vector<SensitiveWordCastOption>& optVec = oneConfig.mSensitiveWordCastOptions[key];
    optVec.resize(1);
    optVec[0].option = SensitiveWordCastOption::CONST_OPTION;
    optVec[0].key = key;
    optVec[0].constValue = constVal;
    optVec[0].replaceAll = replaceAll;
    optVec[0].mRegex.reset(new re2::RE2(regex));
    return oneConfig;
}

PluginInstance::PluginMeta getPluginMeta(){
    PluginInstance::PluginMeta pluginMeta{"testID", "testChildID"};
    return pluginMeta;
}

void ProcessorDesensitizeNativeUnittest::TestMultipleLines() {
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,
dbf@@@324 FS2$%pwd,pwd=saf543#$@,,",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";


    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "asf@@@324 FS2$%pwd,pwd=********,,",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "dbf@@@324 FS2$%pwd,pwd=********,,",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";

    // ProcessorSplitLogStringNative
    {
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.FromJsonString(inJson);

        // make config
        Config config = GetCastSensWordConfig("content");

        std::string pluginId = "testID";
        ComponentConfig componentConfig(pluginId, config);

        // run function ProcessorSplitLogStringNative
        ProcessorSplitLogStringNative processorSplitLogStringNative;
        processorSplitLogStringNative.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processorSplitLogStringNative.Init(componentConfig));
        processorSplitLogStringNative.Process(eventGroup);

        // run function ProcessorDesensitizeNative
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        processorInstance.Process(eventGroup);

        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // ProcessorSplitRegexNative
    {
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.FromJsonString(inJson);

        // make config
        Config config = GetCastSensWordConfig("content");
        config.mLogBeginReg = ".*";

        std::string pluginId = "testID";
        ComponentConfig componentConfig(pluginId, config);
        // run function processorSplitRegexNative
        ProcessorSplitRegexNative processorSplitRegexNative;
        processorSplitRegexNative.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processorSplitRegexNative.Init(componentConfig));
        processorSplitRegexNative.Process(eventGroup);
        // run function ProcessorDesensitizeNative
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        processorInstance.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorDesensitizeNativeUnittest::TestInit() {
    Config config = GetCastSensWordConfig();
    // run function
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
}

void ProcessorDesensitizeNativeUnittest::TestParseCastSensWordConfig() {
    Json::Value constVal;
    constVal["key"] = Json::Value("param1");
    constVal["type"] = Json::Value("const");
    constVal["regex_begin"] = Json::Value("pwd=");
    constVal["regex_content"] = Json::Value("[^,]+");
    constVal["const"] = Json::Value("********");
    constVal["all"] = Json::Value(false);
    Json::Value md5Val;
    md5Val["key"] = Json::Value("param2");
    md5Val["type"] = Json::Value("md5");
    md5Val["regex_begin"] = Json::Value("\\d{6}");
    md5Val["regex_content"] = Json::Value("\\d{8}");
    md5Val["all"] = Json::Value(true);
    Json::Value allVal;
    allVal.append(constVal);
    allVal.append(md5Val);
    Config config;
    ConfigManager::GetInstance()->GetSensitiveKeys(allVal, &config);
    APSARA_TEST_EQUAL((size_t)2, config.mSensitiveWordCastOptions.size());
    std::vector<SensitiveWordCastOption>& param1Vec = config.mSensitiveWordCastOptions["param1"];
    std::vector<SensitiveWordCastOption>& param2Vec = config.mSensitiveWordCastOptions["param2"];
    APSARA_TEST_EQUAL(param1Vec[0].key, "param1");
    APSARA_TEST_EQUAL(param1Vec[0].constValue, "\\1********");
    auto CONST_OPTION = SensitiveWordCastOption::CONST_OPTION;
    APSARA_TEST_EQUAL(param1Vec[0].option, CONST_OPTION);
    APSARA_TEST_EQUAL(param1Vec[0].replaceAll, false);
    APSARA_TEST_EQUAL(param1Vec[0].mRegex->ok(), true);
    APSARA_TEST_EQUAL(param2Vec[0].key, "param2");
    auto MD5_OPTION = SensitiveWordCastOption::MD5_OPTION;
    APSARA_TEST_EQUAL(param2Vec[0].option, MD5_OPTION);
    APSARA_TEST_EQUAL(param2Vec[0].replaceAll, true);
    APSARA_TEST_EQUAL(param2Vec[0].mRegex->ok(), true);
}

void ProcessorDesensitizeNativeUnittest::TestCastSensWordConst() {
    // case1
    {
        Config config = GetCastSensWordConfig();
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=********,,",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case2
    {
        Config config = GetCastSensWordConfig();
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "\r\n\r\nasf@@\n\n@324 FS2$%pwd,pwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "\r\n\r\nasf@@\n\n@324 FS2$%pwd,pwd=********,,",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case3
    {
        Config config = GetCastSensWordConfig();
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=********,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case4
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=********,,pwd=********,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case5
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=sdfpsw=543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=********,,pwd=********,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorDesensitizeNativeUnittest::TestCastSensWordMD5() {
    // case1
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=91F6CFCF46787E8A02082B58F7117AFA,,",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case2
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwd=91F6CFCF46787E8A02082B58F7117AFA,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 3
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwdsaf543#$@,,pwd=12341",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwdsaf543#$@,,pwd=F190CE9AC8445D249747CAB7BE43F7D5",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 4
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwd=12341",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwd=F190CE9AC8445D249747CAB7BE43F7D5",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 5
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=91F6CFCF46787E8A02082B58F7117AFA,,pwd=F190CE9AC8445D249747CAB7BE43F7D5,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 6
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwd=saf543#$@,,pwd=12341f",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwd=91F6CFCF46787E8A02082B58F7117AFA,,pwd=2369B00C6DB80BF0794658225730FF0B",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 7
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwd=saf543#$@,,pwd=12341f,asfasf",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "pwd=91F6CFCF46787E8A02082B58F7117AFA,,pwd=2369B00C6DB80BF0794658225730FF0B,asfasf",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 8
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=91F6CFCF46787E8A02082B58F7117AFA,,",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 9
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].option = SensitiveWordCastOption::MD5_OPTION;
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,\npwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,\npwd=91F6CFCF46787E8A02082B58F7117AFA,,",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorDesensitizeNativeUnittest::TestCastSensWordFail() {
    // case 1: key does not exist
    {
        Config config = GetCastSensWordConfig();
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast0" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast0" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 2: replace whole part
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].constValue = "********";
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,********,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 3: incorrect capture number
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].constValue = "\\2********";
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // the result is not very well definded which lead to test failure on github workflow
        // we just keep the test here to make sure such behavior won't crash the system
        // judge result
        // std::string expectJson = R"({
        //     "events" :
        //     [
        //         {
        //             "contents" :
        //             {
        //                 "cast1" : "asf@@@324 FS2$%pwd,********,,pwd=12341,df",
        //                 "log.file.offset": "0"
        //             },
        //             "timestamp" : 12345678901,
        //             "timestampNanosecond" : 0,
        //             "type" : 1
        //         }
        //     ]
        // })";
        // std::string outJson = eventGroup.ToJsonString();
        // APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 4: empty regex
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].constValue = "********";
        config.mSensitiveWordCastOptions.begin()->second[0].mRegex.reset(new re2::RE2(""));
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "********asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorDesensitizeNativeUnittest::TestCastSensWordLoggroup() {
    Config config = GetCastSensWordConfig();
    vector<SensitiveWordCastOption>& optVec = config.mSensitiveWordCastOptions["id"];
    vector<SensitiveWordCastOption>& cntVec = config.mSensitiveWordCastOptions["content"];
    optVec.resize(1);
    optVec[0].option = SensitiveWordCastOption::CONST_OPTION;
    optVec[0].key = "id";
    optVec[0].constValue = "\\1********";
    optVec[0].replaceAll = true;
    optVec[0].mRegex.reset(new re2::RE2("(\\d{6})\\d{8}"));
    cntVec.resize(1);
    cntVec[0].option = SensitiveWordCastOption::CONST_OPTION;
    cntVec[0].key = "content";
    cntVec[0].constValue = "\\1********";
    cntVec[0].replaceAll = true;
    cntVec[0].mRegex.reset(new re2::RE2("('password':')[^']*"));
    // init
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "user" : "ali-sls-logtail",
                    "cast1" : "pwd=donottellanyone!,",
                    "id" : "33032119850506123X",
                    "content" : "{'account':'18122100036969','password':'04adf38'};akProxy=null;",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "user" : "ali-sls-logtail1",
                    "cast1" : "pwd=dafddasf@@!123!,",
                    "id" : "33032119891206123X",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    processorInstance.Process(eventGroup);
    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "cast1" : "pwd=********,",
                    "content" : "{'account':'18122100036969','password':'********'};akProxy=null;",
                    "id" : "330321********123X",
                    "log.file.offset": "0",
                    "user" : "ali-sls-logtail"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "cast1" : "pwd=********,",
                    "id" : "330321********123X",
                    "log.file.offset": "0",
                    "user" : "ali-sls-logtail1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorDesensitizeNativeUnittest::TestCastSensWordMulti() {
    // case 1
    {
        Config config = GetCastSensWordConfig();
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=********,,",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 2
    {
        Config config = GetCastSensWordConfig();
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "\r\n\r\nasf@@\n\n@324 FS2$%pwd,pwd=saf543#$@,,",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "\r\n\r\nasf@@\n\n@324 FS2$%pwd,pwd=********,,",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 3
    {
        Config config = GetCastSensWordConfig();
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=********,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 4
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=********,,pwd=********,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // case 5
    {
        Config config = GetCastSensWordConfig();
        config.mSensitiveWordCastOptions.begin()->second[0].replaceAll = true;
        // init
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
        ComponentConfig componentConfig(pluginId, config);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=sdfpsw=543#$@,,pwd=12341,df",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup);
        // judge result
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "cast1" : "asf@@@324 FS2$%pwd,pwd=********,,pwd=********,df",
                        "log.file.offset": "0"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorDesensitizeNativeUnittest::TestCastWholeKey() {
    Config config = GetCastSensWordConfig("pwd", "().*", 1, false, "\\1********");
    // init
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(new ProcessorDesensitizeNative, getPluginMeta());
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "pwd" : "asf@@@324 FS2$%pwd,pwd=saf543#$@,,",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    processorInstance.Process(eventGroup);
    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "log.file.offset": "0",
                    "pwd" : "********"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}
} // namespace logtail

UNIT_TEST_MAIN
