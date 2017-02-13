/**
  @file MySim.cpp
*/


/*
<sim2doc>

</sim2doc>
*/


#include <iostream>
#include <cassert>

#include <openfluid/ware/PluggableSimulator.hpp>
#include <openfluid/scientific/FloatingPoint.hpp>
#include <openfluid/tools/ColumnTextParser.hpp>
#include <openfluid/tools/DataHelpers.hpp>


// =====================================================================
// =====================================================================


BEGIN_SIMULATOR_SIGNATURE("water.surf-uz.runoff-infiltration.bvservice")

  // Informations
  DECLARE_NAME("")
  DECLARE_DESCRIPTION("")
  DECLARE_VERSION("")
  DECLARE_STATUS(openfluid::ware::EXPERIMENTAL)


  DECLARE_REQUIRED_EXTRAFILE("landuse2CN.fr.txt")

  DECLARE_USED_PARAMETER("totalrain","total rainfall","m")
  DECLARE_USED_PARAMETER("SUinfiltcoeff","coefficient to apply to all potential infiltrations on SU","")
  DECLARE_USED_PARAMETER("LIinfiltcoeff","coefficient to apply to all potential infiltrations on LI","")

  DECLARE_PRODUCED_ATTRIBUTE("CN","SU","","")

  DECLARE_REQUIRED_ATTRIBUTE("landuse","SU","","")
  DECLARE_REQUIRED_ATTRIBUTE("area","SU","","")

  DECLARE_REQUIRED_ATTRIBUTE("length","LI","length of the linear interface","m")
  DECLARE_REQUIRED_ATTRIBUTE("grassbsratio","LI","ratio of the length which is grass strips","m")
  DECLARE_REQUIRED_ATTRIBUTE("hedgesratio","LI","ratio of the length which is hedges","m")


  DECLARE_PRODUCED_VARIABLE("rain","SU","","m")
  DECLARE_PRODUCED_VARIABLE("infiltration","SU","","m")
  DECLARE_PRODUCED_VARIABLE("uprunoffvolume","SU","incoming runoff volume","m3")
  DECLARE_PRODUCED_VARIABLE("runoffvolume","SU","outgoing runoff volume","m3")
  DECLARE_PRODUCED_VARIABLE("infiltvolume","SU","","m3")

  DECLARE_PRODUCED_VARIABLE("runoffvolume","LI","outgoing runoff volume","m3")
  DECLARE_PRODUCED_VARIABLE("uprunoffvolume","LI","incoming runoff volume","m3")
  DECLARE_PRODUCED_VARIABLE("infiltvolume","LI","","m3")

  DECLARE_PRODUCED_VARIABLE("uprunoffvolume","RS","incoming runoff volume","m3")


END_SIMULATOR_SIGNATURE


// =====================================================================
// =====================================================================


/**

*/
class BVServiceHydroSimulator : public openfluid::ware::PluggableSimulator
{
  private:

    double m_TotalRainM = 0.5; // total rainfall in meters

    double m_SUInfiltCoeff = 1.0;
    double m_LIInfiltCoeff = 1.0;

    double m_LIWidth = 1.0;

    openfluid::core::IDIntMap m_CNofSU;

    const std::map<std::string,int> m_CNbyLIType = {
                                                     {"benches",58},  // like MEADOW
                                                     {"grassbs",69},  // like PASTURE
                                                     {"hedges",58},  // like FOREST
                                                   };


  public:


    BVServiceHydroSimulator(): PluggableSimulator()
    {


    }


    // =====================================================================
    // =====================================================================


    ~BVServiceHydroSimulator()
    {


    }


    // =====================================================================
    // =====================================================================


    /**
      Computes S from CN value (using metric system, S in returned in meters)
    */
    static double computeS(const unsigned int& CN)
    {
      return ((25.4/double(CN))-0.254);
    }


    // =====================================================================
    // =====================================================================


    static double computeRunoff(const double& WaterHeight, const double& S)
    {
      double Runoff = std::pow(WaterHeight-(0.2*S),2)/(WaterHeight+(0.8*S));

      // Low incoming water levels may generate more runoff than incoming water
      // In this case, all incoming water is put to runoff
      if (Runoff > WaterHeight)
        Runoff = WaterHeight;

      return Runoff;
    }


    // =====================================================================
    // =====================================================================


    /**
      Computes incoming runoff volume from upstream SU and LI for the given spatial unit
    */
    double computeUpstreamRunoffVolume(openfluid::core::SpatialUnit* U)
    {
      double UpRunoffVol = 0.0;
      openfluid::core::SpatialUnit* UpU;
      openfluid::core::DoubleValue Q = 0.0;

      // incoming from LI
      openfluid::core::UnitsPtrList_t* UpList = U->fromSpatialUnits("LI");
      if (U)
      {
        OPENFLUID_UNITSLIST_LOOP(UpList,UpU)
        {
          OPENFLUID_GetVariable(UpU,"runoffvolume",Q);
          UpRunoffVol += Q;
        }
      }

      // incoming from SU
      UpList = U->fromSpatialUnits("SU");
      if (U)
      {
        OPENFLUID_UNITSLIST_LOOP(UpList,UpU)
        {
          OPENFLUID_GetVariable(UpU,"runoffvolume",Q);

          UpRunoffVol += Q;
        }
      }

      return UpRunoffVol;
    }


    // =====================================================================
    // =====================================================================


    double computeRunoffVolumeOnLI(openfluid::core::SpatialUnit* U, double& IncomingWaterVolume)
    {
      // 1. Find max ratio
      // 2. Efficient water volume = incoming water volume * max ratio
      // 3. Efficient area = length * max ratio * m_LIWidth;
      // 4. Initial water height = Efficient water volume / Efficient area
      // 5. Compute successive infiltration volume through LI subparts where

      static const std::list<std::string> Subparts = {"benches","grassbs","hedges"};

      double MaxRatio = 0.0;

      for (auto LinearPart : Subparts)
      {
        double Ratio = 0.0;
        OPENFLUID_GetAttribute(U,LinearPart+"ratio",Ratio);

        MaxRatio = std::max(MaxRatio,Ratio);
      }


      double Length = 0.0;
      OPENFLUID_GetAttribute(U,"length",Length);

      std::string OrigID;
      OPENFLUID_GetAttribute(U,"origid",OrigID);
      //std::cout << U->getID() << " : origid = " << OrigID << std::endl;


      double RunoffVolumeToFilter = IncomingWaterVolume * MaxRatio;
      double BypassedRunoffVolume = IncomingWaterVolume - RunoffVolumeToFilter;

      double EfficientArea = Length * MaxRatio * m_LIWidth;

      std::cout.precision(8);

/*      std::cout << U->getID() << " : Area = " << (Length*m_LIWidth) << std::endl;
      std::cout << U->getID() << " : MaxRatio = " << (MaxRatio) << std::endl;
      std::cout << U->getID() << " : EfficientArea = " << EfficientArea << std::endl;*/

      double CurrentRunoff = RunoffVolumeToFilter / EfficientArea;

      std::cout << U->getID() << " : CurrentRunoff-in = " << CurrentRunoff << std::endl;

      if (CurrentRunoff > 0)
      {
        for (auto LinearPart : Subparts)
        {
          double Ratio = 0.0;
          OPENFLUID_GetAttribute(U,LinearPart+"ratio",Ratio);

          if (Ratio > 0.01)
          {
            CurrentRunoff = computeRunoff(CurrentRunoff,computeS(m_CNbyLIType.at(LinearPart)));
            std::cout << U->getID() << " : CurrentRunoff-" << LinearPart << " = " << CurrentRunoff << std::endl;
          }
        }
      }

      double FilteredRunoffVolume = CurrentRunoff * EfficientArea;

      return FilteredRunoffVolume+BypassedRunoffVolume;

    }


    // =====================================================================
    // =====================================================================



    void initParams(const openfluid::ware::WareParams_t& Params)
    {
      OPENFLUID_GetSimulatorParameter(Params,"totalrain",m_TotalRainM);

      OPENFLUID_GetSimulatorParameter(Params,"SUinfiltcoeff",m_SUInfiltCoeff);
      OPENFLUID_GetSimulatorParameter(Params,"LIinfiltcoeff",m_LIInfiltCoeff);
    }


    // =====================================================================
    // =====================================================================


    void prepareData()
    {
      std::map<std::string,int> LandUse2CN;

      // Read landuse2CN codes
      std::string InputDir;
      OPENFLUID_GetRunEnvironment("dir.input",InputDir);

      openfluid::tools::ColumnTextParser LandUse2CNFile("#",";");
      LandUse2CNFile.loadFromFile(InputDir+"/landuse2CN.fr.txt");

      std::string Code;
      long int CN = 0;

      for (unsigned int i = 0;i<LandUse2CNFile.getLinesCount();i++ )
      {
        if (LandUse2CNFile.getStringValue(i,0,&Code) &&
            LandUse2CNFile.getLongValue(i,1,&CN))
        {
          LandUse2CN[Code] = CN;
        }
        else
        {
          OPENFLUID_RaiseError("Format error or wrong value in file landuse2CN.fr.txt");
        }
      }

      // Create CN attribute on SU with corresponding values
      openfluid::core::SpatialUnit* U;

      OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
      {
        std::string LandUseCode = OPENFLUID_GetAttribute(U,"landuse")->toString();

        auto it = LandUse2CN.find(LandUseCode);

        if (it != LandUse2CN.end())
          m_CNofSU[U->getID()] = (*it).second;
        else
        {
          std::string IDStr = openfluid::tools::convertValue(U->getID());
          OPENFLUID_RaiseError("Land use code \"" + LandUseCode + "\" on SU#" + IDStr + " is not valid");
        }
      }
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

      openfluid::core::SpatialUnit* U;

      OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
      {
        OPENFLUID_InitializeVariable(U,"rain",0.0);
        OPENFLUID_InitializeVariable(U,"uprunoffvolume",0.0);
        OPENFLUID_InitializeVariable(U,"infiltration",0.0);
        OPENFLUID_InitializeVariable(U,"runoffvolume",0.0);
        OPENFLUID_InitializeVariable(U,"infiltvolume",0.0);
      }

      OPENFLUID_UNITS_ORDERED_LOOP("LI",U)
      {
        OPENFLUID_InitializeVariable(U,"runoffvolume",0.0);
        OPENFLUID_InitializeVariable(U,"uprunoffvolume",0.0);
        OPENFLUID_InitializeVariable(U,"infiltvolume",0.0);
      }

      OPENFLUID_UNITS_ORDERED_LOOP("RS",U)
      {
        OPENFLUID_InitializeVariable(U,"uprunoffvolume",0.0);
      }

      return DefaultDeltaT();
    }


    // =====================================================================
    // =====================================================================


    openfluid::base::SchedulingRequest runStep()
    {

      openfluid::core::SpatialUnit* U;

      OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
      {
        OPENFLUID_AppendVariable(U,"rain",m_TotalRainM);
      }


      OPENFLUID_ALLUNITS_ORDERED_LOOP(U)
      {
        if (U->getClass() == "SU")
        {
          // Total incoming water = m_TotalRainM + (UpstreamRunoffVolume / Area)
          double Area;
          OPENFLUID_GetAttribute(U,"area",Area);

          double UpstreamRunoffVolume = computeUpstreamRunoffVolume(U);

          double IncomingWaterHeight = m_TotalRainM + (UpstreamRunoffVolume / Area);

          double Runoff = computeRunoff(IncomingWaterHeight,computeS(m_CNofSU[U->getID()]));

          double Infiltration = IncomingWaterHeight - Runoff;

          OPENFLUID_AppendVariable(U,"runoffvolume",Runoff*Area);
          OPENFLUID_AppendVariable(U,"infiltration",Infiltration);
          OPENFLUID_AppendVariable(U,"infiltvolume",Infiltration*Area);
          OPENFLUID_AppendVariable(U,"uprunoffvolume",UpstreamRunoffVolume);
        }
        else if (U->getClass() == "LI")
        {
          // Area = length * m_LIWidth
          // Total incoming water = UpstreamRunoffVolume / Area

          double UpstreamRunoffVolume = computeUpstreamRunoffVolume(U);
          double RunoffVolume = computeRunoffVolumeOnLI(U,UpstreamRunoffVolume);
          double InfiltVolume = UpstreamRunoffVolume - RunoffVolume;

          std::cout << U->getID() << " : " << UpstreamRunoffVolume << "(invol) = " << RunoffVolume << "(runoffvol) + " << InfiltVolume << "(infiltvol)" << std::endl;

          OPENFLUID_AppendVariable(U,"runoffvolume",RunoffVolume);
          OPENFLUID_AppendVariable(U,"infiltvolume",InfiltVolume);
          OPENFLUID_AppendVariable(U,"uprunoffvolume",UpstreamRunoffVolume);
        }
        else if (U->getClass() == "RS")
        {
          double UpstreamRunoffVolume = computeUpstreamRunoffVolume(U);
          OPENFLUID_AppendVariable(U,"uprunoffvolume",UpstreamRunoffVolume);
        }
      }


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


DEFINE_SIMULATOR_CLASS(BVServiceHydroSimulator);


DEFINE_WARE_LINKUID(WARE_LINKUID)
