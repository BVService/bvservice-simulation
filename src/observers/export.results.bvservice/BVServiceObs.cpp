/**
  @file BVServiceObs.cpp
*/


#include <ogrsf_frmts.h>

#include <openfluid/ware/PluggableObserver.hpp>


// =====================================================================
// =====================================================================


BEGIN_OBSERVER_SIGNATURE("export.results.bvservice")



END_OBSERVER_SIGNATURE


// =====================================================================
// =====================================================================



class VarExportInfo
{
  public:

    openfluid::core::Value::Type VarType = openfluid::core::Value::Type::NONE;

    openfluid::core::VariableName_t VarName;

    std::string FieldName;

    VarExportInfo(openfluid::core::Value::Type VT, const openfluid::core::VariableName_t& VN, const std::string& FN) :
      VarType(VT),VarName(VN),FieldName(FN)
    {

    }
};


/**

*/
class BVServiceObserver : public openfluid::ware::PluggableObserver
{
  private:

    std::string m_ESFshapefile;


  public:

    BVServiceObserver() : PluggableObserver()
    {
      std::setlocale(LC_NUMERIC, "C");
    }

    // =====================================================================
    // =====================================================================


    ~BVServiceObserver()
    {

    }


    // =====================================================================
    // =====================================================================


    void initParams(const openfluid::ware::WareParams_t& Params)
    {

    }


    // =====================================================================
    // =====================================================================


    void onPrepared()
    {

    }


    // =====================================================================
    // =====================================================================


    void onInitializedRun()
    {

    }


    // =====================================================================
    // =====================================================================


    void onStepCompleted()
    {

    }


    // =====================================================================
    // =====================================================================


    void prepareExportOfVars(OGRLayer* Layer, const std::vector<VarExportInfo>& Infos)
    {
      for (auto& Info : Infos)
      {
        if (Info.VarType == openfluid::core::Value::Type::DOUBLE)
        {
          OGRFieldDefn TmpField(Info.FieldName.c_str(),OFTReal);
          Layer->CreateField(&TmpField);
        }
        else if (Info.VarType == openfluid::core::Value::Type::INTEGER)
        {
          OGRFieldDefn TmpField(Info.FieldName.c_str(),OFTInteger);
          Layer->CreateField(&TmpField);
        }
      }
    }


    // =====================================================================
    // =====================================================================


    void performExportOfVars(OGRFeature *Feature, openfluid::core::SpatialUnit* U, const std::vector<VarExportInfo>& Infos)
    {
      for (auto& Info : Infos)
      {
        if (Info.VarType == openfluid::core::Value::Type::DOUBLE)
        {
          double Value = OPENFLUID_GetLatestVariable(U,Info.VarName).value()->asDoubleValue();
          if (!std::isnan(Value))
            Feature->SetField(Info.FieldName.c_str(),Value);
        }
        else if (Info.VarType == openfluid::core::Value::Type::INTEGER)
        {
          int Value = OPENFLUID_GetLatestVariable(U,Info.VarName).value()->asIntegerValue();
          Feature->SetField(Info.FieldName.c_str(),Value);
        }
      }
    }


    // =====================================================================
    // =====================================================================


    void onFinalizedRun()
    {


      std::vector<VarExportInfo> SUVarsToExport =
      {
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"upperarea","upareasum"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"runoffvolume","runoffv"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"uprunoffvolume","uprunoffv"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"runoffvoldelta","runoffvd"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"runoffvolratio","runoffvr"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"infiltvolratio","infiltvrd"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"conndegree","conndeg"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"erosionrisk","erosrisk"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"runoffcontrib","runoffctrb"),
        VarExportInfo(openfluid::core::Value::Type::INTEGER,"bufferscount","buffcount")
      };

      std::vector<VarExportInfo> LIVarsToExport =
      {
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"upperarea","upareasum"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"runoffvolume","runoffv"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"uprunoffvolume","uprunoffv"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"runoffvoldelta","runoffvd"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"runoffvolratio","runoffvr"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"infiltvolratio","infiltvrd"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"concdegree","concdeg"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"importancedegree","impdeg"),
        VarExportInfo(openfluid::core::Value::Type::DOUBLE,"interestdegree","intdeg")
      };



      OGRRegisterAll();

      std::string OutputDir;
      OPENFLUID_GetRunEnvironment("dir.output",OutputDir);


      OGRSFDriver* WriteDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("ESRI Shapefile");

      if (WriteDriver)
      {

        std::string SUResultsFullPath = OutputDir+"/SUresults.shp";
        std::string LIResultsFullPath = OutputDir+"/LIresults.shp";

        // TODO test if exists before deleting, using Open() method


        OGRDataSource* TmpSource =  nullptr;
        TmpSource = OGRSFDriverRegistrar::Open(LIResultsFullPath.c_str());
        if (TmpSource)
        {
          OGRDataSource::DestroyDataSource(TmpSource);
          WriteDriver->DeleteDataSource(LIResultsFullPath.c_str());
        }

        TmpSource =  nullptr;
        TmpSource = OGRSFDriverRegistrar::Open(SUResultsFullPath.c_str());
        if (TmpSource)
        {
          OGRDataSource::DestroyDataSource(TmpSource);
          WriteDriver->DeleteDataSource(SUResultsFullPath.c_str());
        }


         // SU results layer

        OGRDataSource* SUresults = WriteDriver->CreateDataSource(SUResultsFullPath.c_str(),nullptr);

        if (SUresults)
        {
          OGRLayer* Layer = SUresults->CreateLayer("SUresults",nullptr, wkbPolygon,nullptr);

          OGRFieldDefn OfldIDField("OFLD_ID",OFTInteger);
          Layer->CreateField(&OfldIDField);

          OGRFieldDefn OrigIDField("origid",OFTString);
          Layer->CreateField(&OrigIDField);

          OGRFieldDefn OutletField("isoutlet",OFTString);
          Layer->CreateField(&OutletField);

          OGRFieldDefn LeafField("isleaf",OFTString);
          Layer->CreateField(&LeafField);


          prepareExportOfVars(Layer,SUVarsToExport);


          openfluid::core::SpatialUnit* U;

          OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
          {
            std::string OrigID = OPENFLUID_GetAttribute(U,"origid")->toString();

            OGRFeature *Feature = OGRFeature::CreateFeature(Layer->GetLayerDefn());

            Feature->SetField("OFLD_ID",int(U->getID()));
            Feature->SetField("origid",OrigID.c_str());
            Feature->SetField("isoutlet",OPENFLUID_GetAttribute(U,"isoutlet")->toString().c_str());
            Feature->SetField("isleaf",OPENFLUID_GetAttribute(U,"isleaf")->toString().c_str());

            performExportOfVars(Feature,U,SUVarsToExport);

            Feature->SetGeometry(U->geometry());

            Layer->CreateFeature(Feature);

            OGRFeature::DestroyFeature(Feature);
          }

          OGRDataSource::DestroyDataSource(SUresults);
        }



      // LI results layer


        OGRDataSource* LIresults = WriteDriver->CreateDataSource(LIResultsFullPath.c_str(),nullptr);

        if (LIresults)
        {
          OGRLayer* Layer = LIresults->CreateLayer("LIresults",nullptr, wkbLineString,nullptr);

          OGRFieldDefn OfldIDField("OFLD_ID",OFTInteger);
          Layer->CreateField(&OfldIDField);

          OGRFieldDefn OrigIDField("origid",OFTString);
          Layer->CreateField(&OrigIDField);

          prepareExportOfVars(Layer,LIVarsToExport);

          openfluid::core::SpatialUnit* U;

          OPENFLUID_UNITS_ORDERED_LOOP("LI",U)
          {
            std::string OrigID = OPENFLUID_GetAttribute(U,"origid")->toString();

            OGRFeature *Feature = OGRFeature::CreateFeature(Layer->GetLayerDefn());

            Feature->SetField("OFLD_ID",int(U->getID()));
            Feature->SetField("origid",OrigID.c_str());

            performExportOfVars(Feature,U,LIVarsToExport);

            Feature->SetGeometry(U->geometry());

            Layer->CreateFeature(Feature);

            OGRFeature::DestroyFeature(Feature);
          }

          OGRDataSource::DestroyDataSource(LIresults);
        }
      }


      // global indicators

      std::ofstream GlobalFile(OutputDir+"/global_indicators.json");

      if (GlobalFile.is_open())
      {
        GlobalFile << "{\n";

        openfluid::core::SpatialUnit* U;


        // global rain volume

        double RainVol = 0.0;
        double CatchmentArea = 0.0;

        OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
        {
          double Area = OPENFLUID_GetAttribute(U,"area")->asDoubleValue();
          double Rain = OPENFLUID_GetLatestVariable(U,"rain").value()->asDoubleValue();

          CatchmentArea += Area;
          RainVol += (Area*Rain);
        }

        // runoff ratio at catchment scale

        double CatchmentContribArea = 0.0;
        double CatchmentRunoffVol = 0.0;

        OPENFLUID_UNITS_ORDERED_LOOP("RS",U)
        {
          double RunoffVol = OPENFLUID_GetLatestVariable(U,"uprunoffvolume").value()->asDoubleValue();
          double ContribArea = OPENFLUID_GetLatestVariable(U,"upperarea").value()->asDoubleValue();

          CatchmentContribArea += ContribArea;
          CatchmentRunoffVol += RunoffVol;
        }


        // avoided runoff landcover

        double AvoidedRunoffVolLinear = 0.0;

        OPENFLUID_UNITS_ORDERED_LOOP("LI",U)
        {
          double RunoffVolDelta = OPENFLUID_GetLatestVariable(U,"runoffvoldelta").value()->asDoubleValue();

          AvoidedRunoffVolLinear += RunoffVolDelta;
        }


        // avoided runoff landcover

        double AvoidedRunoffVolLandcover = 0.0;

        OPENFLUID_UNITS_ORDERED_LOOP("LI",U)
        {
          double RunoffVolDelta = OPENFLUID_GetLatestVariable(U,"runoffvoldelta").value()->asDoubleValue();

          if (RunoffVolDelta < 0)
            AvoidedRunoffVolLandcover += RunoffVolDelta;
        }


        GlobalFile << "  \"catchment_area\" : " << CatchmentArea << ",\n";
        GlobalFile << "  \"catchment_contrib_area\" : " << CatchmentContribArea << ",\n";
        GlobalFile << "  \"rain_volume\" : " << RainVol << ",\n";
        GlobalFile << "  \"catchment_runoff_volume\" : " << CatchmentRunoffVol << ",\n";
        GlobalFile << "  \"catchment_runoff_ratio\" : " << (CatchmentRunoffVol/RainVol) << ",\n";
        GlobalFile << "  \"avoided_runoff_linear_ratio\" : " << (-AvoidedRunoffVolLinear/RainVol) << ",\n";
        GlobalFile << "  \"avoided_runoff_landcover_ratio\" : " << (-AvoidedRunoffVolLandcover/RainVol) << "\n";


        GlobalFile << "}\n";

      }

      GlobalFile.close();

    }

};


// =====================================================================
// =====================================================================


DEFINE_OBSERVER_CLASS(BVServiceObserver)


DEFINE_WARE_LINKUID(WARE_LINKUID)
