/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2019 fisco-dev contributors.
 */
/** @file SQLBasicAccess.cpp
 *  @author darrenyin
 *  @date 2019-04-24
 */

#include "SQLBasicAccess.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace dev::storage;
using namespace std;

int SQLBasicAccess::Select(h256 hash, int num, 
                const std::string& table, const std::string& key,
                Condition::Ptr condition,Json::Value &respJson)
{
    std::string    strSql = this->BuildQuerySql(table,condition);
    LOG(DEBUG) << "hash:"<<hash.hex()<<" num:"<<num<<" table:"
                    <<table<<" key:"<<key<<" query sql:"<<strSql;
    Connection_T    oConn = m_oConnPool.GetConnection();
    LOG(DEBUG)<<"after GetConnection sql:"<<strSql;
    if(oConn == NULL)
    {
        LOG(ERROR) << "hash:"<<hash.hex()<<" num:"<<num<<" table:"
                    <<table<<" key:"<<key<<" query sql:"<<strSql<<" can not get connection";
        return -1;
    }
    TRY
    {
        PreparedStatement_T oPreSatement = Connection_prepareStatement(oConn,"%s",strSql.c_str());
        LOG(DEBUG)<<"execute preprement sql:"<<strSql;
        uint32_t    dwIndex = 0;
        if (condition)
        {
            for (auto it : *(condition->getConditions()))
            {
                PreparedStatement_setString(oPreSatement,++dwIndex,it.second.right.second.c_str());
                LOG(DEBUG) << "hash:"<<hash.hex()<<" num:"<<num<<" table:"
                        <<table<<" key:"<<key<<" index:"<<dwIndex<<" value:"<<it.second.right.second;
            }
        }
        LOG(DEBUG)<<"parameter select sql:"<<strSql;

        ResultSet_T oResult = PreparedStatement_executeQuery(oPreSatement);
        string  strColumnName;
        int32_t iColumnCnt = ResultSet_getColumnCount(oResult);
        for(int32_t iIndex =1;iIndex <= iColumnCnt;++iIndex)
        {
            strColumnName = ResultSet_getColumnName(oResult, iIndex);
            respJson["result"]["columns"].append(strColumnName);
        }

        while(ResultSet_next(oResult))
        {
            Json::Value oValueJson;
            for(int32_t iIndex =1;iIndex <= iColumnCnt;++iIndex)
            {
                oValueJson.append(ResultSet_getString(oResult,iIndex));
            }
            respJson["result"]["data"].append(oValueJson);
        }
    }
    CATCH (SQLException)
    {
        respJson["result"]["columns"].append("_id_");
        respJson["result"]["columns"].append("_hash_");
        respJson["result"]["columns"].append("_num_");
        respJson["result"]["columns"].append("_status_");
        respJson["result"]["columns"].append("key");
        respJson["result"]["columns"].append("value");
        LOG(ERROR)<<"select exception:";
        m_oConnPool.ReturnConnection(oConn);
        return 0;
    }
    END_TRY;
    m_oConnPool.ReturnConnection(oConn);
    return 0;
}

std::string SQLBasicAccess::BuildQuerySql(
        const std::string& table, Condition::Ptr condition)
{
    std::string strSql = "select * from ";
    strSql.append(table);
    uint32_t    dwIndex = 0;
    if (condition)
    {
        auto conditionmap = *(condition->getConditions());
        auto it = conditionmap.begin();
        for (;it != conditionmap.end();++it)
        {
            if(dwIndex == 0)
            {
                ++dwIndex;
                strSql.append(GenerateConditionSql(" where",it,condition));
            }
            else
            {
                strSql.append(GenerateConditionSql(" and",it,condition));
            }
        }
    }

    return strSql;
}

std::string SQLBasicAccess::GenerateConditionSql(const std::string &strPrefix,
                std::map<std::string, Condition::Range>::iterator &it,
                Condition::Ptr condition)
{
    
    string  strConditionSql = strPrefix;
    if(it->second.left.second == it->second.right.second && it->second.left.first &&
                    it->second.right.first)
    {
        strConditionSql.append(" `").append(it->first).append("`=").append("?");
    }
    else
    {
        if (it->second.left.second != condition->unlimitedField())
        {
            if (it->second.left.first)
            {
                strConditionSql.append(" `").append(it->first).append("`>=").append("?");
            }
            else
            {
                strConditionSql.append(" `").append(it->first).append("`>").append("?");
            }
        }

        if (it->second.right.second != condition->unlimitedField())
        {
            if (it->second.right.first)
            {
                strConditionSql.append(" `").append(it->first).append("`<=").append("?");
            }
            else
            {
                strConditionSql.append(" `").append(it->first).append("`<").append("?");
            }
        }
    }

    LOG(DEBUG)<<"value:"<<it->second.right.second;
    return strConditionSql;
}


std::string SQLBasicAccess::BuildCreateTableSql(
        const std::string &tablename,
        const std::string &keyfield,
        const std::string &valuefield
    )
{
    stringstream ss;
    ss<<"CREATE TABLE IF NOT EXISTS `"<<tablename<<"`(\n";
    ss<<" `_id_` int unsigned auto_increment,\n";
    ss<<" `_hash_` varchar(128) not null,\n";
    ss<<" `_num_` int not null,\n";
    ss<<" `_status_` int not null,\n";
    ss<<"`"<<keyfield<<"` varchar(128) default '',\n";

    LOG(DEBUG)<<"valuefield:"<<valuefield;
    std::vector<std::string> vecSplit;
    boost::split(vecSplit,valuefield, boost::is_any_of(","));
    auto it = vecSplit.begin();
    for(;it != vecSplit.end();++it)
    {
        ss<<"`"<<*it<<"` text default '',\n";
    }
    LOG(DEBUG)<<"valuefield:"<<valuefield;

    ss<<" PRIMARY KEY( `_id_` ),\n";
    ss<<" KEY(`"<<keyfield<<"`),\n";
    ss<<" KEY(`_num_`)\n";
    ss<<")ENGINE=InnoDB default charset=utf8mb4;";

    return ss.str();
}

int SQLBasicAccess::Commit(h256 hash, int num,
            const std::vector<TableData::Ptr>& datas)
{
    LOG(DEBUG)<<" commit hash:"<<hash.hex()<<" num:"<<num;
    char cNum[16] = {0};
    snprintf(cNum,sizeof(cNum),"%u",num);
    if (datas.size() == 0)
    {
        LOG(DEBUG) << "Empty data just return";
        return 0;
    }

    /*create table*/

    /*execute commit operation*/

    int32_t dwRowCount = 0;
    Connection_T    oConn = m_oConnPool.GetConnection();
    TRY
    {
        for (auto it : datas)
        {
            auto tableInfo = it->info;
            std::string strTableName = tableInfo->name;

            if(strTableName == "_sys_tables_")
            {
                for (size_t i = 0; i < it->dirtyEntries->size(); ++i)
                {
                    Entry::Ptr entry = it->dirtyEntries->get(i);
                    auto fields = *entry->fields();
                    string  strCreateTableName = fields["table_name"];
                    string  strKeyField = fields["key_field"];
                    string  strValueField =  fields["value_field"];
                    /*generate create table sql*/

                    string  strSql = BuildCreateTableSql(strCreateTableName,strKeyField,strValueField);
                    LOG(DEBUG)<<"create table table:"<<strCreateTableName
                        <<" keyfield:"<<strKeyField<<" value field:"<<strValueField<<" sql:"<<strSql;
                    Connection_execute(oConn,strSql.c_str());
                }

                for (size_t i = 0; i < it->newEntries->size(); ++i)
                {
                    Entry::Ptr entry = it->newEntries->get(i);
                    auto fields = *entry->fields();
                    string  strCreateTableName = fields["table_name"];
                    string  strKeyField = fields["key_field"];
                    string  strValueField =  fields["value_field"];
                    /*generate create table sql*/

                    string  strSql = BuildCreateTableSql(strCreateTableName,strKeyField,strValueField);
                    LOG(DEBUG)<<"create table table:"<<strCreateTableName
                        <<" keyfield:"<<strKeyField<<" value field:"<<strValueField<<" sql:"<<strSql;
                    Connection_execute(oConn,strSql.c_str());
                }


            }

        }
    }
    CATCH (SQLException)
    {
        LOG(ERROR)<<"create table exception:";
        m_oConnPool.ReturnConnection(oConn);
        return 0;
    }
    END_TRY;

    m_oConnPool.BeginTransaction(oConn);
    TRY
    {
        for (auto it : datas)
        {
            auto tableInfo = it->info;
            std::string strTableName = tableInfo->name;
            std::vector<std::string> oVecFieldName;
            std::vector<std::string> oVecFieldValue;
            bool    bHasGetField = false;
            /*different rows*/
            for (size_t i = 0; i < it->dirtyEntries->size(); ++i)
            {
                Entry::Ptr entry = it->dirtyEntries->get(i);
                /*different fields*/
                for (auto fieldIt : *entry->fields())
                {
                    if(fieldIt.first == "_num_" || fieldIt.first == "_hash_")
                    {
                        continue;
                    }
                    if(i == 0 && !bHasGetField)
                    {
                        oVecFieldName.push_back(fieldIt.first);
                    }
                    oVecFieldValue.push_back(fieldIt.second);
                    LOG(DEBUG)<<"new entry key:"<<fieldIt.first<<" value:"<<fieldIt.second;
                }
                oVecFieldValue.push_back(hash.hex());
                oVecFieldValue.push_back(cNum);
            }

             if(oVecFieldName.size()>0 && !bHasGetField)
            {
                oVecFieldName.push_back("_hash_");
                oVecFieldName.push_back("_num_");  
                bHasGetField = true; 
            }

            for (size_t i = 0; i < it->newEntries->size(); ++i)
            {
                Entry::Ptr entry = it->newEntries->get(i);
                /*different fields*/
                for (auto fieldIt : *entry->fields())
                {
                    if(fieldIt.first == "_num_" || fieldIt.first == "_hash_")
                    {
                        continue;
                    }
                    if(!bHasGetField && i == 0)
                    {
                        oVecFieldName.push_back(fieldIt.first);
                    }
                    oVecFieldValue.push_back(fieldIt.second);
                    LOG(DEBUG)<<"new entry key:"<<fieldIt.first<<" value:"<<fieldIt.second;
                }
                oVecFieldValue.push_back(hash.hex());
                oVecFieldValue.push_back(cNum);
            }
            if(oVecFieldName.size()>0 && !bHasGetField)
            {
                oVecFieldName.push_back("_hash_");
                oVecFieldName.push_back("_num_");  
                bHasGetField = true; 
            }

            /*build commit sql*/
            string  strSql = this->BuildCommitSql(strTableName,oVecFieldName,oVecFieldValue);
            LOG(DEBUG)<<" commit hash:"<<hash.hex()<<" num:"<<num<<" commit sql:"<<strSql;
            PreparedStatement_T oPreSatement = Connection_prepareStatement(oConn,"%s",strSql.c_str());
            int32_t    dwIndex = 0;
            auto itValue = oVecFieldValue.begin();
            for(;itValue != oVecFieldValue.end();++itValue)
            {
                PreparedStatement_setString(oPreSatement,++dwIndex,itValue->c_str());
                LOG(DEBUG)<<" index:"<<dwIndex<<" num:"<<num<<" setString:"<<itValue->c_str();
            }
            PreparedStatement_execute(oPreSatement);

             dwRowCount += (int32_t)PreparedStatement_rowsChanged(oPreSatement);
        }
    }
    CATCH (SQLException)
    {
        LOG(ERROR)<<"insert data exception:";
        m_oConnPool.RollBack(oConn);
        m_oConnPool.ReturnConnection(oConn);
        return 0;
    }
    END_TRY;
    m_oConnPool.Commit(oConn);
    m_oConnPool.ReturnConnection(oConn);
    return dwRowCount;
}


 std::string SQLBasicAccess::BuildCommitSql(
        const std::string& table,
        const std::vector<std::string> &oVecFieldName,
        const std::vector<std::string> &oVecFieldValue)
{
    if(oVecFieldName.size()==0 || oVecFieldName.size() ==0 
        || (oVecFieldValue.size()%oVecFieldName.size()))
    {
        /*throw execption*/
        LOG(ERROR)<<"field size:"<<oVecFieldName.size()<<" value size:"<<oVecFieldValue.size();
    }
    uint32_t    dwColumnSize = oVecFieldName.size();
    std::string strSql = "replace into ";
    strSql.append(table).append("(");
    auto it = oVecFieldName.begin();
    for(;it != oVecFieldName.end();++it)
    {
        strSql.append("`").append(*it).append("`").append(",");
    }
    strSql = strSql.substr(0,strSql.size()-1);
    strSql.append(") values");
    
    LOG(DEBUG)<<"field size:"<<oVecFieldName.size()<<" value size:"<<oVecFieldValue.size();

    uint32_t    dwSize = oVecFieldValue.size();
    for(uint32_t dwIndex = 0;dwIndex <dwSize;++dwIndex)
    {
        if(dwIndex % dwColumnSize ==0)
        {
            strSql.append("(?,");
        }
        else
        {
             strSql.append("?,");
        }
        if(dwIndex % dwColumnSize == (dwColumnSize-1))
        {
            strSql = strSql.substr(0,strSql.size()-1);
            strSql.append("),");
        }
    }
    strSql = strSql.substr(0,strSql.size()-1);
    return strSql;
}


void SQLBasicAccess::initConnPool( const std::string &dbtype,
        const std::string &dbip,
        uint32_t    dbport,
        const std::string &dbusername,
        const std::string &dbpasswd,
        const std::string &dbname,
        const std::string &dbcharset,
        uint32_t    initconnections,
        uint32_t    maxconnections)
{
    m_oConnPool.InitConnectionPool(
        dbtype,dbip,
        dbport,dbusername,
        dbpasswd,dbname,
        dbcharset,
        initconnections,maxconnections);
    return;
}
