/*

Overview of the CPL library LAMMPS interface
=============================================

LAMMPS works through a series of user defined fixes. 
These are objects the user can define by inheretiting from a base class called fix with a pre-definied structure.
These are automatically registered with LAMMPS and instantiated (created when LAMMPS runs).
The use can then defined a range of "hook"* functions with names like post_setup or pre_force, which can be used to inject code where you want (after the setup or before the force calcultation respectivly) in the overall LAMMPS solver. 
The addition code is included in LAMMPS by writing user add on packages which can be included when LAMMPS is complied. 
Provided LAMMPS has been built with the extra package, they can then be used, often by keywords to switch these on from the user input. 

For CPL library, the package is USER_CPL and the code inside includes fix_cpl_init, fix_cpl_force and CPLSocketLAMMPS.
The socket is the bulk of the code which handles getting information from LAMMPS, fix_cpl_force applies the constraint force and fix_cpl_init is the top level function to handle communication and call the CPLSocketLAMMPS/fix_cpl_force routines.


* The concept of a hook here is a programming term for a pre-defined place in a software package when you can stick in your own code. LAMMPS is designed to be almost entirly built up of various user packages so provides hooks to get at almost every possible point in the solver.


Section on fix_cpl_init
============================
For CPL library, the fix we use is fix_cpl_init, which handles setup of the CPL library, using code in the "setup" hook to set up a coupled topology, extracting all required parameters from LAMMPS and sending these via MPI (to coupled code).
The required averages are performed in the post_force hook, then information is exchanged between the codes and the constraint forces applied.

The coupling fix, fix_cpl_init, is turned by by adding a command of this form to LAMMPS input:

    fix ID group-ID cpl/init region all forcetype X sendtype Y

The first two arguments are standard LAMMPS:
ID = user-assigned name for the fix
group-ID = ID of the group of atoms to apply the fix to
The next part, cpl/init, is the "style", i.e. the fixes' name. 
"region all" specifies that the fix is applied to the entire box in space. 
Note that in the granular case, this is clearly consistent (as CFD overlaps the whole domain).
For domain decompositional coupling, region should still be all, as we specify the extents of coupling through the COUPLER.in file. 
The remaining words are the args which are currently as follows:


Section on forcetype 
====================
forcetype X -- This allows you to specify which constraint force system to use. 
    The options include 
        1) test -- A simple test for debugging (sends cell indices)
        2) Flekkoy -- stress based coupling
        3) Velocity -- applies (U_particle - U_CFD) which is the most important part of the constraint of Nie, Chen, E and Robbins (2004)
        4) Drag -- base drag class for granular type drag forces
        5) Di_Felice -- an example granular drag class which applies the Di Felice drag correlation (untested!!)
        6+) This has been design so the user can add anything they want here easily. This process is described below. 


Section on sendtypes
====================

sendtype Y -- Specifiy which data is sent to the CFD solver. 
    This Y can be any combination of inputs (note they are additive)
        1) Pick 'n' mix send types
         a) VEL
         b) NBIN 
         c) STRESS
         d) FORCE
         e) FORCECOEFF
         f) VOIDRATIO
        2) Predefinded collections
         a) velocity --4 values including 3 velocity sums VEL and number of particles NBIN
         b) gran -- which sends voidratio VOIDRATIO and force FORCE 
         c) granfull - is designed for SediFOAM and sends velocity VEL, force FORCE, sum of drag coefficients FORCECOEFF and VOIDRATIO.
    So, for example, you could use gran and append an extra VEL to send VOIDRATIO, FORCE and VEL.

Optional: bndryavg Z -- the location of the bottom cell in domain-decompositional coupling, Z can be either below, above or midplane.


fix_cpl_force
--------------------

The force class have a pre-determined interface, set by an abstract base class, to allow them to always be called from fix_cpl_force in the same way.
Think of an interface like a contract, you guarentee that you will always take the same arguments in and return the same things.
In this case, we have an interface which takes in position, velocity, acceleration, mass, radius (sigma) and interaction strength (epsilon) and works out summed up arrays needed for the particular force type (in the pre_force function) and returns the actual force (in the get_force function).
As each type of coupling force you could want to apply always has the same form:

0) Constructor : Create the force class with all required argments stored in a std::map.
1) pre_force : Get some stuff from the particle system (e.g. add up current void fraction, get latest velocity from CFD solver) 
2) get_force : Use the pre_force information, with the CFD solver information, to get the force on each particle. 

These means we can make use of c++ polymorphism, where we choose which type of force we want based on the forcetype input argment.
The required force type object is instantiated using a "factory" which takes the user input and returns fxyz, a pointer to the appropriate object: 
*/

//Force factory
if (fxyzType.compare("Flekkoy") == 0) {
    fxyz.reset(new CPLForceFlekkoy(9, cfdBuf->shape(1), 
                                      cfdBuf->shape(2), 
                                      cfdBuf->shape(3)));
} else if (fxyzType.compare("test") == 0) {
    fxyz.reset(new CPLForceTest(3, cfdBuf->shape(1), 
                                   cfdBuf->shape(2), 
                                   cfdBuf->shape(3)));
} else if (fxyzType.compare("Velocity") == 0) {
    fxyz.reset(new CPLForceVelocity(3, cfdBuf->shape(1), 
                                       cfdBuf->shape(2), 
                                       cfdBuf->shape(3)));
} else if (fxyzType.compare("Drag") == 0) {
    fxyz.reset(new CPLForceDrag(9, cfdBuf->shape(1), 
                                   cfdBuf->shape(2), 
                                   cfdBuf->shape(3),
                                   arg_map)); 

} else if (fxyzType.compare("Di_Felice") == 0) {
    fxyz.reset(new CPLForceGranular(9, cfdBuf->shape(1), 
                                       cfdBuf->shape(2), 
                                       cfdBuf->shape(3),
                                       arg_map)); 

} else {
    std::string cmd("CPLForce type ");
    cmd += fxyzType + " not defined";
    throw std::runtime_error(cmd);
}

/*
The arg_map is a std::map, basically a set of paired {keywords and values}.
These are obtained directly from the user input taking alternating keywords and values following 
forcetype. For example, if you had specified an input of the form:

    fix 5 all cpl/init region all forcetype Di_Felice Cd 0.0005 overlap false me 1e-4 rho 1e3 sendtype velocity

then arg_map would be built up by parsing the commands after "Di_Felice" as
 Key           value
"Cd"            "0.0005"
"overlap"        "false"
"mu"            "1e-4"
"rho"           "1e3"
and this would be passed to the Di_Felice force type, instantiating a CPLForceGranular force object 
and setting the pointer fxyz to this. 
Once we have this fxyz pointer, it can then be used by looping over all particles in LAMMPS (nlocal here)
*/
    //Pre-force calculation, get quantities from discrete system needed to apply force
	for (int i = 0; i < nlocal; ++i)
	{
   		if (mask[i] & groupbit)
    	{
	        //Get local molecule data
	        mi = rmass[i];
	        radi = radius[i];
	        for (int n=0; n<3; n++){
	            xi[n]=x[i][n]; 
	            vi[n]=v[i][n]; 
	            ai[n]=f[i][n];
	        }

	        // Sum all the weights for each cell.
	        fxyz->pre_force(xi, vi, ai, mi, radi, pot);

    	}
    }


//This pre-force has added all the needed  things for the force you want, so we can simply get the force now:

   // Calculate force and apply
    for (int i = 0; i < nlocal; ++i)
    {
        if (mask[i] & groupbit)
        {

            //Get local molecule data
            mi = rmass[i];
            radi = radius[i];
            for (int n=0; n<3; n++){
                xi[n]=x[i][n]; 
                vi[n]=v[i][n]; 
                ai[n]=f[i][n];
            }

            //Get force from object
            fi = fxyz->get_force(xi, vi, ai, mi, radi, pot);

        }
    }

/*
Section on ndArray
=====================

Coupler library is build on the concept of exchanging 4D uniform grids of data. As a result, the first requirement is to setup a 4D array of data based on the number of cells on a given processes. 

The most useful form of CPL_ndArray is a 4D array, that is a dimensionality (i.e. 3 for a vector, 9 for a stress tensor, 100 for a timeseries) and then the three physical dimensions in space, ordered as nd, icell, jcell, kcell. CPL_ndArray has a range of helper functions which mirror numpy or Fortran style arrays.


Section on CPL_field
=====================

CPL_field extends the ndarray to build up a physical field. This is an encapsulated relationship, the field object contains an array of field data, extending it to include domain extents, functions to interpolate or average values in space. So far, there is no inheritance used with field as we can get the required flexitbility from dynamic dispatch (e.g. add_to_array can include a radius, in which case the fraction inside a cell will be calculated).



Section on CPL_force
=====================

CPL_force combines multiple field classes, including the retrieved values from CFD via a coupled MPI exchange and together with accumulated MD/DEM results, it calculates the required force. Heavy use is made of inheritance here, with a common interface of pre_force and a get_force functions. The different types of coupling force are then created using a factory method and extension of this is as simple as inhereting from the most relevant force class and adapting to your needs.
A rough schematic of the inheretence diagram is included below (note names have been shortened): 

                         CPL_vel
Abstract Base Class     / 
   |          _________/__CPL_test
   |         /         \ 
   |        /           CPL_flekkoy
   v       /
CPL_force /                       __CPL_Di_Felice
          \__ CPL_drag__ CPL_gran/
                      \          \__CPL_Tang_______CPL_with_BVK_correction
                       \            
                        CPL_dragtest

Minimal tutorial on creating new Drag Forces
=============================================

The process of designing an appropriate drag force is given as an example here.
We will outline the process of designing the Ergun drag force,

F = 150.0*e*nu*rho/(pow(e*D, 2.0)) + 1.75*rho()*Ur/(e*D)

where we need to work out both porosity e and the fluid velocity Ur = U_MD-U_CFD.
A far more detailed version is given in the next section. 
The minimal changes are given here, add the following to the
CPL_force.h header file,

*/ 

class CPLForceErgun : public CPLForceGranular {

public:

    //Constructors
    using CPLForceGranular::CPLForceGranular;

    //Force specific things
    double drag_coefficient(double r[], double D, std::vector<double> Ui_v);

};

// and the following code to CPL_force.cpp which changes the drag coefficient

double CPLForceErgun::drag_coefficient(double r[], double D, std::vector<double> Ui_v) {
    double eps = CPLForceGranular::get_eps(r);
    if (eps == 0.0) {
        return 0.0;
    } else {
        return 150.0*eps*(mu/rho)*rho/(pow(eps*D, 2.0)) + 1.75*rho/(eps*D);
    }
}

/*

That's it, provided the force can be defined in terms of position, D and relative velocity
Ur with all fields from pre-force eSum and vSums. 


Extended Tutorial on creating new Drag Forces
==============================================

The process of designing an appropriate drag force is given as an example here.
Much of the code is indentical to existing constraints so would not actually 
be needed, it is included for completeness.
Some things may have changed and also some boilerplate code is cut in the example
so this may not work exactly in the form below.
The user should refer to the examples in CPL_force.cpp and build up using this. 

We will outline the process of designing the Ergun drag force,

F = 150.0*e*nu*rho/(pow(e*D, 2.0)) + 1.75*rho()*Ur/(e*D)

where we need to work out both porosity e and the fluid velocity Ur = U_MD-U_CFD 
from the particle simulation. We use an existing class as a basis, 
choose the one that is closest to your current case: 

1) Let's choose a CPL_force type to inheret, the CPLForceDrag in this case.
To define an inhereitance from, CPLForceGranular class, we use 
class CPLForceErgun : public CPLForceDrag 
and start by defining the class definintion in the header file CPL_force.h
*/

class CPLForceErgun : public CPLForceDrag {

public:

    //Constructors
    CPLForceErgun(CPL::ndArray<double>, map_strstr);
    CPLForceErgun(int, int, int, int, map_strstr);

    //Pre force collection and get force calculation
    // position, velocity, acceleration, mass, radius, interaction
    void pre_force(double r[], double v[], double a[], 
                   double m, double s, double e);
    std::vector<double> get_force(double r[], double v[], double a[], 
                                  double m, double s, double e);

    //Flags for various input options (N.B. specify default values here)
    bool calc_preforce = true;
    bool use_overlap = false;
    bool use_interpolate = false;
    bool use_gradP = true;
    bool use_divStress = false;
    double mu = 0.0008900;
    double rho = 1e3;

    //All internal fields
    std::shared_ptr<CPL::CPLField> nSums;
    std::shared_ptr<CPL::CPLField> vSums;
    std::shared_ptr<CPL::CPLField> eSums;
    std::shared_ptr<CPL::CPLField> FSums;
    std::shared_ptr<CPL::CPLField> FcoeffSums;

private:

    double drag_coefficient();
    void initialisesums(CPL::ndArray<double> f);
    void resetsums();

};

/*
2) Now we have specified the header, we then need to override any routines which are unique to CPLForceErgun in 
the CPL_force.cpp file.
But first, we need to define the constructors of our new class, these take in either the size of the
grid array (local to a processes) or an existing array.
These functions essentially creates the buffer which is used to store data from the CFD (in the parent).
This is referred internally in functions of CPLForceErgun as "fieldptr" and the array data can be obtained.
The child automatically calls the parent constructor and then we explicitly call initialisesums to setup 
various fields which will be populated pre_force.
The logic here is the CPL array expected from the CFD code can be used and all other fields created 
with a consistent size.
*/

//Constructor using cells
CPLForceErgun::CPLForceErgun(int nd, int icells, 
                             int jcells, int kcells, 
                             map_strstr arg_map) 
        : CPLForceDrag(nd, icells, jcells, kcells, map_strstr arg_map)
{
//    unpack_arg_map(arg_map);
//    initialisesums(fieldptr->get_array());
}

//Constructor of datatype
CPLForceErgun::CPLForceErgun(CPL::ndArray<double> arrayin, 
                             map_strstr arg_map) 
        : CPLForceDrag(arrayin, map_strstr arg_map){
//    unpack_arg_map(arg_map);
//    initialisesums(arrayin);
}

/*
Notice the input argmap with type map_strstr which is a map using strings
defined in CPL_force.h. 

typedef std::map <std::string, std::string> map_strstr

This is used to store all user keywords from the input line of LAMMPS (see above).
We need to write the function to unpack these arguments so as to the important 
input values for your new CPL_force.
Recall in the header we had a number of parameters:
*/
    //Flags for various input options (N.B. specify default values here)
    bool use_overlap = false;
    bool use_interpolate = false;
    bool use_gradP = true;
    bool use_divStress = false;
    double mu = 0.0008900;
    double rho = 1e3;
/*
We want to write unpack_arg_map to parse arg_map and over-ride default values with
anything the user has specified. To do this, we can use a number of helper functions.
We loop over arg_map using "for (const auto& arg : arg_map)" and can get
keywords from arg.first and values from arg.second. We can then use 
string_contains(arg.first, keyword) to check if any part of the user
input contains a keyword, for example "overlap", before setting the bool
value in out CPL_force based on the value following the overlap keyword.
We can use checktrue(arg.second) which handles 0/1 or case insensitive true/false
returning the boolean value. This semi-manual method of setting insures the
inputs specified are as required for the Force class we design.
For numerical constants, we need to convert from string to double which is done
using std::stod.
*/
    // Iterate over the map and print out all key/value pairs.
    for (const auto& arg : arg_map)
    {
        if (string_contains(arg.first, "overlap") != -1) {
            use_overlap = checktrue(arg.second);
        } else if (string_contains(arg.first, "interpolate") != -1) {
            use_interpolate = checktrue(arg.second);
        } else if (string_contains(arg.first, "gradP")  != -1) {
            use_gradP = checktrue(arg.second);
        } else if (string_contains(arg.first, "divStress")  != -1) {
            use_divStress = checktrue(arg.second);
        } else if (string_contains(arg.first, "mu")  != -1) {
            mu = std::stod(arg.second);
        } else if (string_contains(arg.first, "rho")  != -1) {
            rho = std::stod(arg.second);
        } else {
            std::cout << "key: " << arg.first << 
            " for forcetype not recognised " << '\n';
            throw std::runtime_error("CPLForceDrag input not recognised");
        }

    }
/*
Note that calc_preforce is not set here. This determines if pre_force is called
and this is not something the user should be able to change (it is essential to get
eSum for the get_force calcultion and turning it off would cause problems). 
The initialisesums also needs to be developed, this can follow 
exactly from CPLForceDrag so we could simply inherit this, however it is here for 
completeness
*/
void CPLForceDrag::initialisesums(CPL::ndArray<double> arrayin){
    
    int i = arrayin.shape(1);
    int j = arrayin.shape(2);
    int k = arrayin.shape(3);
    nSums = std::make_shared<CPL::CPLField>(1, i, j, k, "nSums");
    vSums = std::make_shared<CPL::CPLField>(3, i, j, k, "vSums");
    eSums = std::make_shared<CPL::CPLField>(1, i, j, k, "eSums");
    FSums = std::make_shared<CPL::CPLField>(3, i, j, k, "FSums");
    FcoeffSums = std::make_shared<CPL::CPLField>(1, i, j, k, "FcoeffSums");

    build_fields_list();
    resetsums();
}

void CPLForceDrag::build_fields_list(){

    fields.push_back(nSums);
    fields.push_back(vSums);
    fields.push_back(eSums);
    fields.push_back(FSums);
    fields.push_back(FcoeffSums);
}

/*
3) Next, we need to develop pre force and get_force for the Ergun equation.
As Ergun needs porosity, this must be collected pre-force.
The field classes provide an elegant way to get these (currently not used in most of cpl_force as this is new).
We simply add to an array based on molecular position r, the value we want to, here a count for 
no. of molecules for nSums (1), the sphere volume for eSums and velocity vSums either weighted by
overlap fraction in a cell or not.
Again, nothing is needed here if the code below is sufficient, for Ergun this is the case.
*/


//Pre force collection of sums (should this come from LAMMPS fix chunk/atom bin/3d)
void CPLForceErgun::pre_force(double r[], double v[], double a[], 
                              double m, double s, double e) 
{
    nSums.add_to_array(r, 1.0);
    double vol = (4./3.)*M_PI*pow(s,3);
    if (! use_overlap){
        eSums->add_to_array(r, volume);
        vSums->add_to_array(r, v);
    } else {
        eSums->add_to_array(r, s, volume);
        vSums->add_to_array(r, s, v);
    }
}

/*
If you wanted to use partial overlap calculation (fraction of sphere in box) then simply add the
 radius of the shere in as a second argument,
    eSums.add_to_array(r, s, vol);
the choice between these is set by use_overlap from user input.

4) We now need to work out the value of force defined as a 3 vector.
In principle, the only thing we need to change from CPLdrag is the drag coefficient
routine here,
*/

double CPLForceErgun::drag_coefficient(double r[], double D) {
    //Porosity e is cell volume - sum in volume
    double e = 1.0 - eSums->get_array_value(r)/eSums.dV;
    if (e < 1e-5) {
        std::cout << "Warning: 0 particles in cell (" 
                  << cell[0] << ", " << cell[1] << ", " << cell[2] << ")"
                  << std::endl;
        return 0.0;
    }
    return 150.0*e*nu*rho/(pow(e*D, 2.0)) + 1.75*rho/(e*D);
}

//Which is used as follows,


//Get force using sums collected in pre force
std::vector<double> CPLForceErgun::get_force(double r[], double v[], double a[], 
                                             double m, double s, double e) 
{
    //Check array is the right size
    CPL::ndArray<double>& array = cfd_array_field->get_array_pointer();
    assert(array.shape(0) == 9);

    //Get all elements of recieved field
    if (! use_interpolate){
        //Based on cell
        std::vector<int> indices = {0,1,2}; 
        Ui = cfd_array_field->get_array_value(indices, r);
        for (int &n : indices) n += 3; 
        gradP = cfd_array_field->get_array_value(indices, r);
        for (int &n : indices) n += 3; 
        divStress = cfd_array_field->get_array_value(indices, r);
    } else {
        //Or interpolate to position in space
        std::vector<int> indices = {0,1,2}; 
        Ui = cfd_array_field->get_array_value_interp(indices, r);
        for (int &n : indices) n += 3; 
        gradP = cfd_array_field->get_array_value_interp(indices, r);
        for (int &n : indices) n += 3; 
        divStress = cfd_array_field->get_array_value_interp(indices, r);
    }


    //Get Diameter
    double D = 2.0*s;
    //Get drag coefficient
    double Cd = drag_coefficient(r, D);

    //Calculate force
    CPL::ndArray<int> indices = {0,1,2};
    Ui = fieldptr->get_array_value(indices, r);
    for (int i=0; i<3; i++){
        f[i] = Cd*(Ui[i]-v[i]);
        //Include pressure
        if (use_gradP)
            f[i] += -volume*gradP[i];
        // and stress
        if (use_divStress)
            f[i] += volume*divStress[i];
    }

    // Add sum of coefficients of forces 
    // Needed if you want to split implicit/explicit terms for
    // improved numerical stability according to 
    // Xiao H., Sun J. (2011) Algorithms in a Robust Hybrid
    // CFD-DEM Solver for Particle-Laden Flows, 
    // Commun. Comput. Phys. 9, 2, 297
    FcoeffSums->add_to_array(r, Cd);
    FSums->add_to_array(r, &f[0]);

    return f;

}

/*
5) Finally, we need to add out new force type into the fix_cpl_force
*/

//Force factory
if (fxyzType.compare("Flekkoy") == 0) {
    fxyz.reset(new CPLForceFlekkoy(9, cfdBuf->shape(1), 
                                      cfdBuf->shape(2), 
                                      cfdBuf->shape(3)));
...
...
...
} else if (fxyzType.compare("Ergun") == 0) {
    fxyz.reset(new CPLForceErgun(9, cfdBuf->shape(1), 
                                   cfdBuf->shape(2), 
                                   cfdBuf->shape(3),
                                   arg_map));


/*
and we can turn on by adding Ergun to the input line, something like this,

    fix ID group-ID cpl/init region all forcetype Ergun overlap false interpolate true mu 0.0009 rho 1000 gradP true sendtype Granfull

*/
