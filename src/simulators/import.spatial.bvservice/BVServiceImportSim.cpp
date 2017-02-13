/**
  @file MySim.cpp
*/


/*
<sim2doc>

</sim2doc>
*/


#include <fstream>

#include <ogrsf_frmts.h>

#include <openfluid/ware/PluggableSimulator.hpp>
#include <openfluid/tools/DataHelpers.hpp>


// =====================================================================
// =====================================================================


BEGIN_SIMULATOR_SIGNATURE("import.spatial.bvservice")

  // Informations
  DECLARE_NAME("")
  DECLARE_DESCRIPTION("")
  DECLARE_VERSION("")
  DECLARE_STATUS(openfluid::ware::EXPERIMENTAL)

  DECLARE_REQUIRED_PARAMETER("SUshapefile","Surface entities shapefile name","")
  DECLARE_REQUIRED_PARAMETER("LIshapefile","Linear entities shapefile name","")
  DECLARE_REQUIRED_PARAMETER("RSshapefile","Ditches and river segments shapefile name","")

  DECLARE_USED_PARAMETER("forceRSconnect","Force subtrees to be connected to network","")

  DECLARE_UPDATED_UNITSGRAPH("Creation of spatial graph for BVservice process")
  DECLARE_UPDATED_UNITSCLASS("SU","surface units")
  DECLARE_UPDATED_UNITSCLASS("LI","linear interfaces such as hedges, grass strips, banks, ...")
  DECLARE_UPDATED_UNITSCLASS("RS","ditches and rivers")


  DECLARE_PRODUCED_ATTRIBUTE("origid","SU","original ID","")
  DECLARE_PRODUCED_ATTRIBUTE("origtoid","SU","original ID of the downstream unit","")
  DECLARE_PRODUCED_ATTRIBUTE("area","SU","area of the surface unit","m")
  DECLARE_PRODUCED_ATTRIBUTE("landuse","SU","","")
  DECLARE_PRODUCED_ATTRIBUTE("slopemin","SU","","")
  DECLARE_PRODUCED_ATTRIBUTE("slopemean","SU","","")
  DECLARE_PRODUCED_ATTRIBUTE("slopemax","SU","","")
  DECLARE_PRODUCED_ATTRIBUTE("flowdist","SU","","")
  DECLARE_PRODUCED_ATTRIBUTE("isoutlet","SU","","")
  DECLARE_PRODUCED_ATTRIBUTE("isleaf","SU","","")

  DECLARE_PRODUCED_ATTRIBUTE("origid","LI","original ID","")
  DECLARE_PRODUCED_ATTRIBUTE("origtoid","LI","original ID of the downstream unit","")
  DECLARE_PRODUCED_ATTRIBUTE("length","LI","length of the linear interface","m")
  DECLARE_PRODUCED_ATTRIBUTE("flowdist","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("grassbsratio","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("hedgesratio","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("benchesratio","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("grassbslen","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("hedgeslen","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("bencheslen","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("surftolen","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("isoutlet","LI","","")
  DECLARE_PRODUCED_ATTRIBUTE("isleaf","LI","","")

  DECLARE_PRODUCED_ATTRIBUTE("origid","RS","original ID","")
  DECLARE_PRODUCED_ATTRIBUTE("origtoid","RS","original ID of the downstream unit","")
  DECLARE_PRODUCED_ATTRIBUTE("length","RS","length of the linear interface","m")
  DECLARE_PRODUCED_ATTRIBUTE("flowdist","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("thalwegsratio","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("watercsratio","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("ditchesratio","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("thalwegslen","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("watercslen","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("ditcheslen","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("surftolen","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("isoutlet","RS","","")
  DECLARE_PRODUCED_ATTRIBUTE("isleaf","RS","","")

END_SIMULATOR_SIGNATURE


// =====================================================================
// =====================================================================


class SpatialUnitData
{
  public:

    std::string OrigID;

    std::string OrigToID;

    std::string WktGeometry;

    SpatialUnitData* FromSpatialUnit = nullptr;

    SpatialUnitData* ToSpatialUnit = nullptr;

    openfluid::core::UnitClassID_t ClassID;

    openfluid::core::UnitClassID_t ToClassID;

    openfluid::core::Attributes Attributes;

};



class AttrImportInfo
{
  public:

    OGRFieldType FieldType;

    std::string FieldName;

    openfluid::core::AttributeName_t AttrName;


    AttrImportInfo(OGRFieldType FT, const std::string& FN, const openfluid::core::AttributeName_t& AN) :
      FieldType(FT),FieldName(FN),AttrName(AN)
    { }
};


/**

*/
class BVServiceImportSimulator : public openfluid::ware::PluggableSimulator
{

  private:

    std::string m_SUshapefile;

    std::string m_LIshapefile;

    std::string m_RSshapefile;

    // Map<original ID,data>
    std::map<std::string,SpatialUnitData> m_SpatialUnitsMap;


    int m_MinPcsOrd;

    bool m_ForceRSConnect = false;


  public:


    BVServiceImportSimulator(): PluggableSimulator()
    {


    }


    // =====================================================================
    // =====================================================================


    ~BVServiceImportSimulator()
    {


    }


    // =====================================================================
    // =====================================================================

/*
    void saveEntitiesReport(const std::map<std::string,EntityData>& EMap, const std::string& Filename)
    {
      std::string OutputDir;
      OPENFLUID_GetRunEnvironment("dir.output",OutputDir);

      std::ofstream Report;
      Report.open(OutputDir+"/"+Filename,std::fstream::out);

      Report << "src;origunit;origto;unit;to\n";

      for (auto& Ent : EMap)
      {
        Report << Ent.second.Source << ";";

        Report << Ent.second.OrigID << ";";
        std::string OrigToID = Ent.second.OrigToID;
        if (OrigToID.empty())
          OrigToID = "-";
        Report << OrigToID << ";";


        Report << Ent.second.ClassID.first << "#"<< Ent.second.ClassID.second << ";";
        if (Ent.second.ToClassID.first.empty())
          Report << "-;";
        else
          Report << Ent.second.ToClassID.first << "#"<< Ent.second.ToClassID.second << ";";

        Report << "\n";
      }

       Report.close();
    }
*/

    // =====================================================================
    // =====================================================================


    void importLayer(const std::string& UnitsClass, const std::string& Shapefile, const std::vector<AttrImportInfo>& AttrInfos)
    {
      OGRDataSource* Source = OGRSFDriverRegistrar::Open(Shapefile.c_str(),false);

      if (Source)
      {
        OGRLayer* Layer = Source->GetLayer(0);

        int IDFldIdx = Layer->FindFieldIndex("ID",true);
        if (IDFldIdx  < 0)
          OPENFLUID_RaiseError("Cannot find ID attribute in "+UnitsClass+" shapefile");

        int IDToFldIdx = Layer->FindFieldIndex("IDTo",true);
        if (IDToFldIdx  < 0)
          OPENFLUID_RaiseError("Cannot find IDTo attribute in "+UnitsClass+" shapefile");


        std::map<std::string,int> FieldsIndexes;

        for (auto& Info : AttrInfos)
        {
          int AttrFldIdx = Layer->FindFieldIndex(Info.FieldName.c_str(),true);

          if (AttrFldIdx < 0)
            OPENFLUID_RaiseError("Cannot find "+Info.FieldName+" attribute in "+UnitsClass+" shapefile");

          if (Layer->GetLayerDefn()->GetFieldDefn(AttrFldIdx)->GetType() != Info.FieldType)
            OPENFLUID_RaiseError("Field "+Info.FieldName+" attribute is not of type "+
                                 std::string(OGRFieldDefn::GetFieldTypeName(Info.FieldType))+" in "+UnitsClass+" shapefile");

          FieldsIndexes[Info.FieldName] = AttrFldIdx;
        }

        OGRFeature* Feature = nullptr;
        Layer->ResetReading();

        unsigned int UnitID = 1;
        while ((Feature = Layer->GetNextFeature()) != nullptr)
        {
          std::string OrigID(Feature->GetFieldAsString(IDFldIdx));

          if (m_SpatialUnitsMap.find(OrigID) != m_SpatialUnitsMap.end())
            OPENFLUID_RaiseError("Duplicate entity "+OrigID);

          std::string OrigToID(Feature->GetFieldAsString(IDToFldIdx));
          if (OrigToID == "None")
            OrigToID.clear();

          m_SpatialUnitsMap[OrigID].OrigID = OrigID;
          m_SpatialUnitsMap[OrigID].OrigToID = OrigToID;
          m_SpatialUnitsMap[OrigID].ClassID = {UnitsClass,UnitID};

          char* WktBuffer = nullptr;
          Feature->GetGeometryRef()->exportToWkt(&WktBuffer);
          m_SpatialUnitsMap[OrigID].WktGeometry = std::string(WktBuffer);
          OGRFree(WktBuffer);


          for (auto& Info : AttrInfos)
          {
            if (Info.FieldType == OFTReal)
              m_SpatialUnitsMap[OrigID].Attributes.setValue(Info.AttrName,openfluid::core::DoubleValue(Feature->GetFieldAsDouble(FieldsIndexes[Info.FieldName])));
            else if (Info.FieldType == OFTString)
              m_SpatialUnitsMap[OrigID].Attributes.setValue(Info.AttrName,openfluid::core::StringValue(Feature->GetFieldAsString(FieldsIndexes[Info.FieldName])));
          }


          UnitID++;
        }
      }
      else
        OPENFLUID_RaiseError("Cannot open "+UnitsClass+" shapefile " + Shapefile);

      OGRDataSource::DestroyDataSource(Source);
    }


    // =====================================================================
    // =====================================================================


    void computeProcessOrderRecursively(openfluid::core::SpatialUnit* U, int PcsOrd)
    {
      // postfixed depth-first search to process leafs first and go back to root

     /*OPENFLUID_DisplayInfo("PcsOrd : entering unit " << U->getClass() << "#" << U->getID());*/

      std::set<openfluid::core::SpatialUnit*> UpperUnits;

      openfluid::core::UnitsPtrList_t* TmpUpperList;
      TmpUpperList = U->fromSpatialUnits("SU");
      if (TmpUpperList != nullptr)
      {
        for (auto TmpU : *TmpUpperList)
          UpperUnits.insert(TmpU);
      }


      TmpUpperList = U->fromSpatialUnits("LI");
      if (TmpUpperList != nullptr)
      {
        for (auto TmpU : *TmpUpperList)
          UpperUnits.insert(TmpU);
      }


      openfluid::core::SpatialUnit* UpU = nullptr;

      // --- go to leafs recursively
      if (!UpperUnits.empty()) //if UpperUnits not empty, upstream Units are present
      {
        for(auto UpU : UpperUnits) // loop for each upstream unit
        {
          computeProcessOrderRecursively(UpU,PcsOrd-1);
        }
      }


      /*OPENFLUID_DisplayInfo("PcsOrd : processing unit " << U->getClass() << "#" << U->getID());*/

      U->setProcessOrder(PcsOrd);

      m_MinPcsOrd = std::min(m_MinPcsOrd,PcsOrd);

    }


    // =====================================================================
    // =====================================================================


    void initParams(const openfluid::ware::WareParams_t& Params)
    {
      OPENFLUID_GetSimulatorParameter(Params,"SUshapefile",m_SUshapefile);
      OPENFLUID_GetSimulatorParameter(Params,"LIshapefile",m_LIshapefile);
      OPENFLUID_GetSimulatorParameter(Params,"RSshapefile",m_RSshapefile);

      long Force = 0;
//      OPENFLUID_GetSimulatorParameter(Params,"forceRSconnect",Force);
      m_ForceRSConnect = Force;
    }


    // =====================================================================
    // =====================================================================


    void prepareData()
    {

      OGRRegisterAll();


      if (m_SUshapefile.empty())
        OPENFLUID_RaiseError("SU shapefile path is empty");

      if (m_LIshapefile.empty())
        OPENFLUID_RaiseError("LI shapefile path is empty");

      if (m_LIshapefile.empty())
        OPENFLUID_RaiseError("RS shapefile path is empty");


      importLayer("SU",m_SUshapefile,
                  {
                    AttrImportInfo(OFTReal,"Surface","area"),
                    AttrImportInfo(OFTString,"LandUse","landuse"),
                    AttrImportInfo(OFTReal,"SlopeMin","slopemin"),
                    AttrImportInfo(OFTReal,"SlopeMean","slopemean"),
                    AttrImportInfo(OFTReal,"SlopeMax","slopemax"),
                    AttrImportInfo(OFTReal,"FlowDist","flowdist")
                  });

      importLayer("LI",m_LIshapefile,
                  {
                    AttrImportInfo(OFTReal,"Length","length"),
                    AttrImportInfo(OFTReal,"FlowDist","flowdist"),
                    AttrImportInfo(OFTReal,"Hedges","hedgesratio"),
                    AttrImportInfo(OFTReal,"GrassBs","grassbsratio"),
                    AttrImportInfo(OFTReal,"Benches","benchesratio"),
                    AttrImportInfo(OFTReal,"HedgesL","hedgeslen"),
                    AttrImportInfo(OFTReal,"GrassBsL","grassbslen"),
                    AttrImportInfo(OFTReal,"BenchesL","bencheslen"),
                    AttrImportInfo(OFTReal,"SurfToLen","surftolen")
                  });
      importLayer("RS",m_RSshapefile,
                  {
                    AttrImportInfo(OFTReal,"Length","length"),
                    AttrImportInfo(OFTReal,"FlowDist","flowdist"),
                    AttrImportInfo(OFTReal,"Ditches","ditchesratio"),
                    AttrImportInfo(OFTReal,"Thalwegs","thalwegsratio"),
                    AttrImportInfo(OFTReal,"WaterCs","watercsratio"),
                    AttrImportInfo(OFTReal,"DitchesL","ditcheslen"),
                    AttrImportInfo(OFTReal,"ThalwegsL","thalwegslen"),
                    AttrImportInfo(OFTReal,"WaterCsL","watercslen"),
                    AttrImportInfo(OFTReal,"SurfToLen","surftolen")
                  });


     // Rebuild of map connections

      for (auto itEnt = m_SpatialUnitsMap.begin(); itEnt != m_SpatialUnitsMap.end(); ++itEnt)
      {

        if (!(*itEnt).second.OrigToID.empty() && (*itEnt).second.ToSpatialUnit == nullptr &&
            (*itEnt).second.OrigToID != (*itEnt).second.OrigID)
        {
          auto itToEnt = m_SpatialUnitsMap.find((*itEnt).second.OrigToID);

          if (itToEnt != m_SpatialUnitsMap.end())
          {
            (*itToEnt).second.FromSpatialUnit = &(m_SpatialUnitsMap[(*itEnt).first]);
            (*itEnt).second.ToSpatialUnit = &(m_SpatialUnitsMap[(*itEnt).second.OrigToID]);
            (*itEnt).second.ToClassID = (*itToEnt).second.ClassID;
          }
        }
      }


      // mark outlets and leafs
      for (auto itEnt = m_SpatialUnitsMap.begin(); itEnt != m_SpatialUnitsMap.end(); ++itEnt)
      {
        if ((*itEnt).second.ToSpatialUnit)
          (*itEnt).second.Attributes.setValue("isoutlet",openfluid::core::BooleanValue(false));
        else
          (*itEnt).second.Attributes.setValue("isoutlet",openfluid::core::BooleanValue(true));

        if ((*itEnt).second.FromSpatialUnit)
          (*itEnt).second.Attributes.setValue("isleaf",openfluid::core::BooleanValue(false));
        else
          (*itEnt).second.Attributes.setValue("isleaf",openfluid::core::BooleanValue(true));
      }



      // Creation of spatial units
      for (auto& Ent : m_SpatialUnitsMap)
      {
        OPENFLUID_AddUnit(Ent.second.ClassID.first,Ent.second.ClassID.second,1);

        openfluid::core::SpatialUnit* U = OPENFLUID_GetUnit(Ent.second.ClassID.first,Ent.second.ClassID.second);

        if (U)
        {
          U->importGeometryFromWkt(Ent.second.WktGeometry);

          OPENFLUID_SetAttribute(U,"origid",Ent.second.OrigID);
          OPENFLUID_SetAttribute(U,"origtoid",Ent.second.OrigToID);

          for (auto& Attr : Ent.second.Attributes.getAttributesNames())
          {
            OPENFLUID_SetAttribute(U,Attr,*(Ent.second.Attributes.value(Attr)));
          }
        }
      }


      // Creation of connections
      for (auto& Ent : m_SpatialUnitsMap)
      {
        if (OPENFLUID_IsUnitExist(Ent.second.ClassID.first,Ent.second.ClassID.second) &&
            OPENFLUID_IsUnitExist(Ent.second.ToClassID.first,Ent.second.ToClassID.second))
        {
          OPENFLUID_AddFromToConnection(Ent.second.ClassID.first,Ent.second.ClassID.second,Ent.second.ToClassID.first,Ent.second.ToClassID.second);
        }
      }


      // Compute of process orders

      openfluid::core::SpatialUnit* U;
      std::vector<openfluid::core::SpatialUnit*> Outlets;


      OPENFLUID_ALLUNITS_ORDERED_LOOP(U)
      {
        if (U->toSpatialUnits("SU") == nullptr && U->toSpatialUnits("RS") == nullptr && U->toSpatialUnits("LI") == nullptr)
          Outlets.push_back(U);
      }


      for (auto OutU : Outlets)
      {
//        OPENFLUID_DisplayInfo("PcsOrd : processing outlet " << OutU->getClass() << "#" << OutU->getID());

        m_MinPcsOrd = 0;

        computeProcessOrderRecursively(OutU,0); // first recursive pass to detect the minimal process order

        computeProcessOrderRecursively(OutU,-m_MinPcsOrd+1); // second recursive pass to shift all process orders as the minimal is now one

      }

      mp_SpatialData->sortUnitsByProcessOrder();



/*

      // Rebuild of entities connections
      auto itEntClean = EntitiesMap.begin();

      for (auto itEnt = EntitiesMap.begin(); itEnt != EntitiesMap.end(); ++itEnt)
      {

        if (!(*itEnt).second.OrigToID.empty() && (*itEnt).second.ToEntity == nullptr &&
            (*itEnt).second.OrigToID != (*itEnt).second.OrigID)
        {
          auto itToEnt = EntitiesMap.find((*itEnt).second.OrigToID);

          if (itToEnt != EntitiesMap.end())
          {
            (*itToEnt).second.FromEntity = &(EntitiesMap[(*itEnt).first]);
            (*itEnt).second.ToEntity = &(EntitiesMap[(*itEnt).second.OrigToID]);
            (*itEnt).second.ToClassID = (*itToEnt).second.ClassID;
          }
        }
      }


      // Removal of  disconnected entities
      itEntClean = EntitiesMap.begin();
      while (itEntClean != EntitiesMap.end())
      {
        if ((*itEntClean).second.ClassID.first != "SU" &&
            (*itEntClean).second.ToEntity == nullptr && (*itEntClean).second.FromEntity == nullptr)
        {
          itEntClean = EntitiesMap.erase(itEntClean);
        }
        else
        {
          ++itEntClean;
        }
      }


      OPENFLUID_DisplayInfo(OPENFLUID_GetUnitsCount("SU") << " SU units has been created");
      OPENFLUID_DisplayInfo(OPENFLUID_GetUnitsCount("LI") << " LI units has been created");
      OPENFLUID_DisplayInfo(OPENFLUID_GetUnitsCount("RS") << " RS units has been created");



      openfluid::core::SpatialUnit* U;

      // ---------- force orphaned subtrees to be connected to a virtual RS

      if (m_ForceRSConnect)
      {
        OPENFLUID_AddUnit("RS",0,1);
        openfluid::core::SpatialUnit* RS0 = OPENFLUID_GetUnit("RS",0);
        OPENFLUID_SetAttribute(RS0,"origid",openfluid::core::StringValue("none"));
        OPENFLUID_SetAttribute(RS0,"origtoid",openfluid::core::StringValue("none"));
        OPENFLUID_SetAttribute(RS0,"length",openfluid::core::DoubleValue());
        OPENFLUID_DisplayInfo("RS#0 created");


        OPENFLUID_DisplayInfo("Trying to connect orphans to network");

        OPENFLUID_ALLUNITS_ORDERED_LOOP(U)
        {
          OPENFLUID_DisplayInfo("Testing unit " << U->getClass() << "#" << U->getID() << " as orphaned");

          if (U->getClass() != "RS" && U->toSpatialUnits("SU") == nullptr && U->toSpatialUnits("RS") == nullptr && U->toSpatialUnits("LI") == nullptr)
          {

            OPENFLUID_DisplayInfo("Unit " << U->getClass() << "#" << U->getID() << " is orphaned");

            OPENFLUID_AddFromToConnection(U->getClass(),U->getID(),"RS",0);
          }
        }
      }


      // ---------- reporting

      //saveEntitiesReport(EntitiesMap,"post.csv");
*/

    }


    // =====================================================================
    // =====================================================================


    void checkConsistency()
    {


    }


    // =====================================================================
    // =====================================================================


    openfluid::base::SchedulingRequest initializeRun()
    {

      return DefaultDeltaT();
    }


    // =====================================================================
    // =====================================================================


    openfluid::base::SchedulingRequest runStep()
    {

      return DefaultDeltaT();
    }


    // =====================================================================
    // =====================================================================


    void finalizeRun()
    {


    }

};


// =====================================================================
// =====================================================================


DEFINE_SIMULATOR_CLASS(BVServiceImportSimulator);


DEFINE_WARE_LINKUID(WARE_LINKUID)
