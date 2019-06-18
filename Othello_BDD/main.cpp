#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <locale>
#include <math.h>
#include <time.h>
#include <vector>

extern "C"{
  #include <sylvan.h>
  #include <lace.h>
  #include <sylvan_int.h>
  #include <sylvan_table.h>
}

using namespace std;
using namespace sylvan;



#define L 4 //number of rows
#define Wi 5 //number of columns
#define  DL   (L*Wi)                     // index to deadlock relation


static BDD          P[2][2];            // player:  0/1  X (un)primed
static BDD          F[L][Wi][4][2];      // fields: field X field X color X (un)primed
static BDDSET       VA[L*Wi+1],          // var set for partial input states
                    VB[L*Wi+1],          // var set for partial output states (primed)
                    VR[L*Wi+1],          // var set for partial relations
                    VA0,                // full var set (unprimed)
                    VB0,                // full var set (primed)
                    VR0;                // full var set of total relation
static vector<int>  varsArray;
bool odd = L%2 || Wi%2;

enum {
    W = 0, // White
    B = 1, // Black
    E = 2, // Empty
    A = 3, // Any
    //N = 4, // Not possible
};

static int NOUT = 0;
VOID_TASK_0(gc_start)
{
    if (NOUT) return;
    double nodes_size = 24 * llmsset_get_size(nodes) / (1024 * 1024 * 1024);
    double cache_size = 36 * cache_getsize() / (1024 * 1024 * 1024);
    printf("(GC) Starting garbage collection ...\t\t%'5.0fGB / %'5.0fGB\n", nodes_size, cache_size);
}

VOID_TASK_0(gc_end)
{
    if (NOUT) return;
    double nodes_size = 24 * llmsset_get_size(nodes) / (1024 * 1024 * 1024);
    double cache_size = 36 * cache_getsize() / (1024 * 1024 * 1024);
    printf("(GC) Garbage collection done.\t\t\t%'5.0fGB / %'5.0fGB\n", nodes_size, cache_size);
}

VOID_TASK_4(printboard, void*, ctx, BDDVAR*, VA, uint8_t*, values, int, count) {
    int prev = 1;
    /*for(int i = 0; i<count;i++){
        cout << VA[i] << ", ";
        if(values[i] == 0){
           cout << "0" << endl;
        }
        else{
            cout << "1" << endl;
        }
    }*/
    if(values[0]==0){
        cout << "Player 0 (white) has turn.." << endl; 
    }
    else cout << "Player 1 (black) has turn.." << endl;
    for (int i = 1; i < count; i++) {
        if(i%2==1){
            prev = values[i];
            //cout << " i ";
        }
        else{
            if(prev == 0 && values[i] == 0){
                printf("-"); // empty
            }
            else if(prev == 1 && values[i] == 0){
                printf("W");
            }
            else if(prev == 0 && values[i] == 1){
                printf("."); //not possible (assume empty)
            }
            else{
                printf("B");
            }
        }
        if (i % (2*Wi) == 0) printf("\n");
    }
    printf("\n");
}
#define printboard TASK(printboard)

VOID_TASK_4(printrelboards, void*, ctx, BDDVAR*, VR, uint8_t*, values, int, count) {
    int prev = 1;
    if(values[0]==0){
        cout << "Player 0 (white) has turn.." << endl; 
    }
    else cout << "Player 1 (black) has turn.." << endl;
    if(values[1]==0){
        cout << "Player 0 (white) has turn next.." << endl; 
    }
    else cout << "Player 1 (black) has turn next.." << endl;
    cout << "Old board: " << endl;
    int countprints = 0;
    for (int i = 2; i < count; i++) {
        int start = VR[i-1];
        if(i==2){
            start = 4;
        }
        for(size_t a=start;a<VR[i]-1;a+=4){
            cout << "U"; //unknown
            countprints++;
            if (countprints % Wi == 0) cout << endl;
        }
        if(i==count-1){
            for(int a=VR[i];a<L*Wi*4+2;a+=4){ 
                cout << "U"; //unknown
                countprints++;
                if (countprints % Wi == 0) cout << endl;
            }
        }
        if(i%4==1 || i%4==3){
            continue;
        }
        if(i%4==2){
            prev = values[i];
        }
        else{
            countprints++;
            if(prev == 0 && values[i] == 0){
                cout << "E";
            }
            else if(prev == 1 && values[i] == 0){
                cout << "W";
            }
            else if(prev == 0 && values[i] == 1){
                cout << "."; //not possible
            }
            else{
                cout << "B";
            }
        if (countprints % Wi == 0) cout << endl;
        }
    }
    cout << endl << "New board: " << endl;
    countprints = 0;
    for (int i = 3; i < count; i++) {
        int start = VR[i-1];
        if(i==3){
            start = 5;
        }
        for(size_t a=start;a<VR[i]-1;a+=4){
            cout << "U"; //unknown
            countprints++;
            if (countprints % Wi == 0) cout << endl;
        }
        if(i%4==0 || i%4==2){
            continue;
        }
        if(i%4==3){
            prev = values[i];
        }
        else{
            countprints++;
            if(prev == 0 && values[i] == 0){
                cout << "E";
            }
            else if(prev == 1 && values[i] == 0){
                cout << "W";
            }
            else if(prev == 0 && values[i] == 1){
                cout << ".";//not possible
            }
            else{
                cout << "B";
            }
        if (countprints % Wi == 0) cout << endl;
        }
        if(i==count-1){
            for(int a=VR[i];a<L*Wi*4+2;a+=4){ 
                cout << "U"; //unknown
                countprints++;
                if (countprints % Wi == 0) cout << endl;
            }
        }
    }
    cout << endl;
}
#define printrelboards TASK(printrelboards)

void
print_support(BDD level)
{
    LACE_ME;
    BDD old = sylvan_support(level);
    bdd_refs_pushptr(&old);
    while (!sylvan_set_isempty(old)) {
        printf("%d, ", sylvan_set_first(old));
        old = sylvan_set_next(old);
    }
    bdd_refs_popptr(1);
    printf("\n");
}

BDD s_and(BDD a, BDD b) {
    LACE_ME;
    bdd_refs_pushptr(&a);
    bdd_refs_pushptr(&b);
    BDD temp = sylvan_and(a, b);
    bdd_refs_popptr(2);
    return temp;
}

BDD s_or(BDD a, BDD b) {
    LACE_ME;
    bdd_refs_pushptr(&a);
    bdd_refs_pushptr(&b);
    BDD temp = sylvan_or(a, b);
    bdd_refs_popptr(2);
    return temp;
    //mtbdd_pop(2)
}

// @input add: array of added bits (set to sylvan_invalid to overwrite, otherwise equals is used)
// TODO: try sylvan_maps
BDD
add_or_sub(BDD *add, BDD *x, BDD *y, int l, bool sub)
{
    LACE_ME;
    if (l == 0) {
        return sylvan_false;            // carry out (carry in to lsb)
    } else {
        BDD cin = add_or_sub(add + 1, x + 1, y + 1, l - 1, sub);
        bdd_refs_push(cin);
        BDD xor1 = sylvan_xor(*x, *y);
        bdd_refs_push(xor1);
        BDD bit = sylvan_xor(xor1, cin);
        bdd_refs_push(bit);
        if (*add == sylvan_invalid) {
            *add = bit;
        } else {
            *add = sylvan_ite(*add, bit, sylvan_not(bit));
        }
        BDD and1 = sylvan_and(sub ? sylvan_not(*x) : *x, *y); // half adder carry/borrow
        bdd_refs_push(and1);
        BDD and2 = sylvan_and(sub ? sylvan_not(xor1) : xor1, cin);
        bdd_refs_push(and2);
        cin = sylvan_or(and1, and2); // full adder carry/borrow
        bdd_refs_pop(5);
        return cin;                 // carry out
    }
}

void calcVarOrder(){
    //original order
    for (int i=0; i<L*Wi; i++) {
        varsArray.push_back(i);
    }

    //squaresorder
    /*int rings = min(L,Wi)/2;
    for(int ring=0;ring<rings;ring++){
        varsArray.push_back(Wi*ring+ring);//linksboven
        varsArray.push_back(Wi*ring+Wi-ring-1);//rechtsboven
        varsArray.push_back(Wi*(L-1-ring)+ring);//linksonder
        varsArray.push_back(Wi*(L-1-ring)+Wi-ring-1);//rechtsonder
        for(int j=ring+1;j<Wi-ring-1;j++){//north side
            varsArray.push_back(Wi*ring+j);
        }
        for(int j=ring+1;j<Wi-ring-1;j++){//south side
            varsArray.push_back(Wi*(L-ring-1)+j);
        }
        for(int i=ring+1;i<L-ring-1;i++){//west side
            varsArray.push_back(Wi*i+ring);
        }
        for(int i=ring+1;i<L-ring-1;i++){//east side
            varsArray.push_back(Wi*i+L-ring-1);
        }
    }*/
    //BDD upside down squares order
    /*
    for(int ring=0;ring<rings;ring++){
        varsArray.insert(varsArray.begin(), Wi*ring+ring);//linksboven
        varsArray.insert(varsArray.begin(), Wi*ring+Wi-ring-1);//rechtsboven
        varsArray.insert(varsArray.begin(), Wi*(L-1-ring)+ring);//linksonder
        varsArray.insert(varsArray.begin(), Wi*(L-1-ring)+Wi-ring-1);//rechtsonder
        for(int j=ring+1;j<Wi-ring-1;j++){//north side
            varsArray.insert(varsArray.begin(), Wi*ring+j);
        }
        for(int j=ring+1;j<Wi-ring-1;j++){//south side
            varsArray.insert(varsArray.begin(), Wi*(L-ring-1)+j);
        }
        for(int i=ring+1;i<L-ring-1;i++){//west side
            varsArray.insert(varsArray.begin(), Wi*i+ring);
        }
        for(int i=ring+1;i<L-ring-1;i++){//east side
            varsArray.insert(varsArray.begin(), Wi*i+L-ring-1);
        }
    }*/
}

void
init_othello()
{
    printf("Othello on a %dx%d board.\n", L, Wi);

    LACE_ME;
    //turn vars top of BDD
    for (int i=0; i<L*Wi; i++){
        VA[i] = sylvan_set_empty();
        sylvan_protect (&VA[i]);
        VB[i] = sylvan_set_empty();
        sylvan_protect (&VB[i]);
        VR[i] = sylvan_set_empty();
        sylvan_protect (&VR[i]);

        VA[i] = sylvan_set_add(VA[i], 0);
        VB[i] = sylvan_set_add(VB[i], 1);
        VR[i] = sylvan_set_add(VR[i], 0);
        VR[i] = sylvan_set_add(VR[i], 1);
    }

    VA0 = sylvan_set_empty();
    sylvan_protect (&VA0);
    VB0 = sylvan_set_empty();
    sylvan_protect (&VB0);
    VR0 = sylvan_set_empty();
    sylvan_protect (&VR0);

    VA0 = sylvan_set_add(VA0, 0);
    VB0 = sylvan_set_add(VB0, 1);
    VR0 = sylvan_set_add(VR0, 0);
    VR0 = sylvan_set_add(VR0, 1);

    for (int b = 1; b >= 0; b--) {
        P[0][b] = sylvan_nithvar(b);
        sylvan_protect (P[0]);
        P[1][b] = sylvan_ithvar(b);
        sylvan_protect (P[1]);
    }
    //turn vars bottom of BDD
    /*for(int i=0;i<L*Wi;i++){
        VA[i] = sylvan_set_empty();
        sylvan_protect (&VA[i]);
        VB[i] = sylvan_set_empty();
        sylvan_protect (&VB[i]);
        VR[i] = sylvan_set_empty();
        sylvan_protect (&VR[i]);

        VA[i] = sylvan_set_add(VA[i], L*Wi*4+4);
        VB[i] = sylvan_set_add(VB[i], L*Wi*4+4+1);
        VR[i] = sylvan_set_add(VR[i], L*Wi*4+4);
        VR[i] = sylvan_set_add(VR[i], L*Wi*4+4+1);
    }

    VA0 = sylvan_set_empty();
    sylvan_protect (&VA0);
    VB0 = sylvan_set_empty();
    sylvan_protect (&VB0);
    VR0 = sylvan_set_empty();
    sylvan_protect (&VR0);

    VA0 = sylvan_set_add(VA0, L*Wi*4+4);
    VB0 = sylvan_set_add(VB0, L*Wi*4+4+1);
    VR0 = sylvan_set_add(VR0, L*Wi*4+4);
    VR0 = sylvan_set_add(VR0, L*Wi*4+4+1);

    for (int b = 1; b >= 0; b--) {
        P[0][b] = sylvan_nithvar(L*Wi*4+4+b);
        sylvan_protect (P[0]);
        P[1][b] = sylvan_ithvar(L*Wi*4+4+b);
        sylvan_protect (P[1]);
    }*/
    sylvan_gc_disable();
    for (int i = L-1; i >= 0; i--) { // add vars to sets bottom up
        for (int j = Wi-1; j >= 0; j--) {
            vector<int>::iterator it = find(varsArray.begin(), varsArray.end(), Wi*i+j);
            int v = distance(varsArray.begin(), it)*4;
            //int v = i*4*Wi + j*4;
            //int v = i<<5 | j<<2; //second to last bit is for v1/v2, last bit for the primed / unprimed
            v = v+4; //since v shouldnt get 0, since in P are already ithvars and nithvars for 0
            for (int b = 1; b >= 0; b--) {
                int v1 = v     | b;
                int v2 = v | 2 | b;
                //cout << "v: " << v << " v1: " << v1 << " v2: " << v2 << "for i and j: " << i << ", " << j << endl;

                F[i][j][E][b] = sylvan_nithvar(v1);
                //F[i][j][E][b] = sylvan_and(sylvan_nithvar(v1), sylvan_nithvar(v2));
                F[i][j][W][b] = sylvan_and(sylvan_ithvar(v1) , sylvan_nithvar(v2));
                F[i][j][B][b] = sylvan_and(sylvan_ithvar(v1) , sylvan_ithvar(v2));
                F[i][j][A][b] =            sylvan_ithvar(v1);
                //F[i][j][N][b] = sylvan_and(sylvan_nithvar(v1), sylvan_ithvar(v2));
                sylvan_protect (&F[i][j][E][b]);
                sylvan_protect (&F[i][j][W][b]);
                sylvan_protect (&F[i][j][B][b]);
                sylvan_protect (&F[i][j][A][b]);
                //sylvan_protect (&F[i][j][N][b]);
            }

            VA0 = sylvan_set_add(VA0, v|0b00);   //unprimed 1
            VA0 = sylvan_set_add(VA0, v|0b10);   //unprimed 2
            VB0 = sylvan_set_add(VB0, v|0b01);   //  primed 1
            VB0 = sylvan_set_add(VB0, v|0b11);   //  primed 2
            VR0 = sylvan_set_add(VR0, v|0b00);
            VR0 = sylvan_set_add(VR0, v|0b10);
            VR0 = sylvan_set_add(VR0, v|0b01);
            VR0 = sylvan_set_add(VR0, v|0b11);
            int max_z = min(L,Wi);
            for(int x=0;x<L;x++){
                for(int y=0;y<Wi;y++){
                    for(int z=1;z<max_z;z++){
                        if(x==i || y==j || ((x==i-z || x==i+z) && (y==j-z || y==j+z))){
                            it = find(varsArray.begin(), varsArray.end(), Wi*x+y);
                            v = distance(varsArray.begin(), it)*4;
                            //v = x*4*Wi + y*4;
                            //v = x<<5 | y<<2;
                            v = v+4;
                            VA[Wi*i+j] = sylvan_set_add(VA[Wi*i+j], v|0b00);   //unprimed 1
                            VA[Wi*i+j] = sylvan_set_add(VA[Wi*i+j], v|0b10);   //unprimed 2
                            VB[Wi*i+j] = sylvan_set_add(VB[Wi*i+j], v|0b01);   //  primed 1
                            VB[Wi*i+j] = sylvan_set_add(VB[Wi*i+j], v|0b11);   //  primed 2
                            VR[Wi*i+j] = sylvan_set_add(VR[Wi*i+j], v|0b00);
                            VR[Wi*i+j] = sylvan_set_add(VR[Wi*i+j], v|0b10);
                            VR[Wi*i+j] = sylvan_set_add(VR[Wi*i+j], v|0b01);
                            VR[Wi*i+j] = sylvan_set_add(VR[Wi*i+j], v|0b11);
                        }
                    }
                }
            }
        }
    }

    // deadlock relation
    VA[DL] = VA0;
    VB[DL] = VB0;
    VR[DL] = VR0;
    sylvan_protect (&VA[DL]);
    sylvan_protect (&VB[DL]);
    sylvan_protect (&VR[DL]);

    sylvan_gc_enable();
    //bdd_refs_popptr(1);
}

BDD starting_position(int starting, int pos){
    LACE_ME;
    BDD start = sylvan_true;
    bdd_refs_pushptr(&start);
    start = sylvan_and(start, P[starting][0]);
    if(!odd){
        for (int i = L-1; i >= 0; i--) { // bottom up
            for (int j = Wi-1; j >= 0; j--) {
                if((i==L/2-1 && j==Wi/2-1) || (i==L/2 && j==Wi/2)){
                    start = sylvan_and(start, F[i][j][B][0]);
                }
                else if((i==L/2 && j==Wi/2-1) || (i==L/2-1 && j==Wi/2)){
                    start = sylvan_and(start, F[i][j][W][0]);
                }
                else{
                    start = sylvan_and(start, F[i][j][E][0]);
                }
            }
        }
    }
    else{
        for (int i = L-1; i >= 0; i--) { // bottom up
            for (int j = Wi-1; j >= 0; j--) {
                if(Wi*i+j == pos || Wi*i+j == pos+Wi+1){
                    start = sylvan_and(start, F[i][j][B][0]);
                }
                else if(Wi*i+j == pos+1 || Wi*i+j == pos+Wi){
                    start = sylvan_and(start, F[i][j][W][0]);
                }
                else{
                    start = sylvan_and(start, F[i][j][E][0]);
                }
            }
        }
    }
    bdd_refs_popptr(1);
    return start;
}

BDD boardWithXStones(int stones){
    LACE_ME;

    int numbits = floor(log2(L*Wi))+1;
    BDD sum[3][numbits];
    BDD out = sylvan_false;
    bdd_refs_pushptr(&out);
    BDD temp = sylvan_false;
    bdd_refs_pushptr(&temp);
    for (int i = 0; i < 3; i++) {
       for (int b = 0; b < numbits; b++) {
           sum[i][b] = sylvan_false;
           bdd_refs_pushptr(&sum[i][b]);
       }
    }

    for (int i = L*Wi-1; i >= 0; i--) {
        for(int bit=0;bit<numbits-2;bit++){
            sum[2][bit] = sylvan_false;
        }
        sum[2][numbits-1] = F[(int)floor(i/Wi)][i%Wi][E][0];
        if (i == 0) {
            for(int bit = 0;bit<numbits;bit++){
                if((L*Wi-stones)>>(numbits-1-bit) & 1){
                    sum[!(i&1)][bit] = sylvan_true; 
                }   
                else{
                    sum[!(i&1)][bit] = sylvan_false;
                }
            }
            //sum[!(i&1)][0] = sum[!(i&1)][1] = sum[!(i&1)][2] = sum[!(i&1)][3] = sum[!(i&1)][4] = sylvan_true;
        } else {
            for(int bit=0;bit<numbits;bit++){
                sum[!(i&1)][bit] = sylvan_invalid;
            }
        }
        temp = add_or_sub(sum[!(i&1)], sum[i&1], sum[2], numbits, false);
        out = sylvan_or(temp, out);
    }

    out = sylvan_not(out);
    for (int b = 0; b < numbits; b++) {
        out = sylvan_and(sum[1][b], out);
    }
    //out = sylvan_and(out, winning);
    bdd_refs_popptr(3*numbits+2);
    //CALL(sylvan_enum, out, VA0, printboard, NULL);
    return out;
}

BDD moreWhiteStones(){
    LACE_ME;
    BDD winning = sylvan_false;
    bdd_refs_pushptr(&winning);
    int numbits = floor(log2(ceil(L*Wi/2)-1))+1;
    BDD sum[3][numbits];
    BDD out = sylvan_false;
    bdd_refs_pushptr(&out);
    BDD temp = sylvan_false;
    bdd_refs_pushptr(&temp);
    for(int k=ceil(L*Wi/2)-1;k>=0;k--){
        for (int i = 0; i < 3; i++) {
           for (int b = 0; b < numbits; b++) {
               sum[i][b] = sylvan_false;
               bdd_refs_pushptr(&sum[i][b]);
           }
        }

        out = sylvan_false;
        temp = sylvan_false;
        for (int i = L*Wi-1; i >= 0; i--) {
            for(int bit=0;bit<numbits-2;bit++){
                sum[2][bit] = sylvan_false;
            }
            sum[2][numbits-1] = F[(int)floor(i/Wi)][i%Wi][B][0];
            if (i == 0) {
                for(int bit = 0;bit<numbits;bit++){
                    if(k>>(numbits-1-bit) & 1){
                        sum[!(i&1)][bit] = sylvan_true; 
                    }   
                    else{
                        sum[!(i&1)][bit] = sylvan_false;
                    }
                }
                //sum[!(i&1)][0] = sum[!(i&1)][1] = sum[!(i&1)][2] = sum[!(i&1)][3] = sum[!(i&1)][4] = sylvan_true;
            } else {
                for(int bit=0;bit<numbits;bit++){
                    sum[!(i&1)][bit] = sylvan_invalid;
                }
            }
            temp = add_or_sub(sum[!(i&1)], sum[i&1], sum[2], numbits, false);
            out = sylvan_or(temp, out);
        }

        out = sylvan_not(out);
        for (int b = 0; b < numbits; b++) {
            out = sylvan_and(sum[1][b], out);
        }
        for(int a=2*k+1;a<L*Wi+1;a++){
            winning = s_or(winning, sylvan_and(out, boardWithXStones(a)));
        }
        bdd_refs_popptr(3*numbits);
    }
    bdd_refs_popptr(3);//out en temp en winning
    return winning;
}
BDD one_direction_enclose(int p, int i, int j, int encloseri, int encloserj){
    BDD temp1 = sylvan_true;
    bdd_refs_pushptr(&temp1);
    bool east = j<encloserj;
    bool south = i<encloseri;
    bool west = j>encloserj;
    bool north = i>encloseri;
    LACE_ME;
    int dist = 0;
    if(north || south){
        dist = abs(i-encloseri);
    }
    else{
        dist = abs(j-encloserj);
    }
    int enclosedrow, enclosedcol;
    if(south){
        enclosedrow = i;
        enclosedcol = j;
    }
    else{
        enclosedrow = encloseri;
        enclosedcol = encloserj;
    }
    for(int i=0;i<dist-1;i++){
        if(south || north){
            enclosedrow++;
        }
        if((west&&!south)||(east&&south)){
            enclosedcol++;
        }
        else if((east&&!south)||(west&&south)){
            enclosedcol--;
        }
        temp1 = sylvan_and(temp1, F[enclosedrow][enclosedcol][!p][0]); // until encloser must be other players pieces  
        temp1 = sylvan_and(temp1, F[enclosedrow][enclosedcol][p][1]); // until encloser swap sides (get yours) 
    }
    int copyi = encloseri-1*north+1*south;
    int copyj = encloserj-1*west+1*east;
    while(copyi<L && copyi>=0 && copyj<Wi && copyj>=0){//rest of current direction remains unchanged
        temp1 = s_and(temp1, s_or(sylvan_and(F[copyi][copyj][E][0], F[copyi][copyj][E][1]), s_or(sylvan_and(F[copyi][copyj][B][0], F[copyi][copyj][B][1]), sylvan_and(F[copyi][copyj][W][0], F[copyi][copyj][W][1]))));
        if(south){
            copyi++;
        }
        else if(north){
            copyi--;
        }
        if(east){
            copyj++;
        }
        else if(west){
            copyj--;
        } 
    }
    temp1 = sylvan_and(temp1, F[encloseri][encloserj][p][0]); // encloser must be yours
    temp1 = sylvan_and(temp1, F[encloseri][encloserj][p][1]); // encloser remains unchanged
    bdd_refs_popptr(1);
    return temp1;
}

BDD
init_othello_transition_relation (int i, int j)
{
    BDD rel = sylvan_true;
    bdd_refs_pushptr(&rel);
    BDD rel2 = sylvan_false;
    bdd_refs_pushptr(&rel2);
    BDD relFirstPlayer = sylvan_true;
    bdd_refs_pushptr(&relFirstPlayer);

    BDD temp1 = sylvan_false;
    bdd_refs_pushptr(&temp1);
    BDD temp2 = sylvan_true;
    bdd_refs_pushptr(&temp2);
    BDD toRemove = sylvan_true; // fill this by concatenating the 'unchanged BDD' for all directions
    bdd_refs_pushptr(&toRemove);
    BDD reduceUnchanged = sylvan_true;//reduce unchanged (temp1) by removing cases where something will change in that direction
    //since we have: pair of boards(a,b) ok when in every direction of a, exists encloser or complete direction unchanged, but not both. Otherwise it could be the case
    //that there exists an encloser but nothing changes. So the move is ok, but it is not applied. 
    bdd_refs_pushptr(&reduceUnchanged);

    // build relation
    LACE_ME;

    for (int p = 0; p <= 1; p++) {
        rel = sylvan_true;
        temp2 = s_and(sylvan_and(P[p][0], P[!p][1]), F[i][j][E][0]); // fieldToPlay must be empty & turn should change | for every iteration, so outside loop
        temp2 = sylvan_and(temp2, F[i][j][p][1]); // fieldToPlay gets yours | for every iteration, so outside loop

        //ENCLOSE TO EAST
        for(int east=j+2;east<Wi;east++){ // all enclosers east of fieldToPlay
            temp1 = s_and(temp2, one_direction_enclose(p,i,j,i,east));
            rel2 = sylvan_or(rel2, temp1); //true when exists any western encloser 
        }
        temp1 = temp2;
        for(int copy=j+1;copy<Wi;copy++){ //east unchanged 
            temp1 = s_and(temp1, s_or(sylvan_and(F[i][copy][E][0], F[i][copy][E][1]), s_or(sylvan_and(F[i][copy][B][0], F[i][copy][B][1]), sylvan_and(F[i][copy][W][0], F[i][copy][W][1]))));
        }
        for(int east=j+2;east<Wi;east++){ // all enclosers east of fieldToPlay must be removed from temp1
            for(int walker=j+1;walker<east;walker++){
                reduceUnchanged = sylvan_and(reduceUnchanged, F[i][walker][!p][0]);
            }
            reduceUnchanged = sylvan_and(reduceUnchanged, F[i][east][p][0]);
            temp1 = sylvan_diff(temp1, reduceUnchanged); //now: row can only be unchanged when not exists western encloser
            reduceUnchanged = sylvan_true;
        }
        toRemove = sylvan_and(toRemove, temp1); //we have to remove the case when in all 8 directions nothing changes. Since there must be at least one encloser.
        rel2 = sylvan_or(rel2, temp1); // true when exists any western encloser or complete western row unchanged, but not both
        rel = sylvan_and(rel,rel2);
        rel2 = sylvan_false;

        //ENCLOSE TO SOUTH
        for(int south=i+2;south<L;south++){ // all enclosers south of fieldToPlay
            temp1 = s_and(temp2, one_direction_enclose(p,i,j,south,j));
            rel2 = sylvan_or(rel2, temp1);
        }
        temp1 = temp2;
        for(int copy=i+1;copy<L;copy++){ //south unchanged
            temp1 = s_and(temp1, s_or(sylvan_and(F[copy][j][E][0], F[copy][j][E][1]), s_or(sylvan_and(F[copy][j][B][0], F[copy][j][B][1]), sylvan_and(F[copy][j][W][0], F[copy][j][W][1]))));
        }
        for(int south=i+2;south<L;south++){ // all enclosers south of fieldToPlay must be removed from temp1
            for(int walker=i+1;walker<south;walker++){
                reduceUnchanged = sylvan_and(reduceUnchanged, F[walker][j][!p][0]);
            }
            reduceUnchanged = sylvan_and(reduceUnchanged, F[south][j][p][0]);
            temp1 = sylvan_diff(temp1, reduceUnchanged);
            reduceUnchanged = sylvan_true;
        }
        toRemove = sylvan_and(toRemove, temp1);
        rel2 = sylvan_or(rel2, temp1);
        rel = sylvan_and(rel,rel2);
        rel2 = sylvan_false;

        //ENCLOSE TO WEST
        for(int west=j-2;west>=0;west--){ // all enclosers west of fieldToPlay
            temp1 = s_and(temp2, one_direction_enclose(p,i,j,i,west));
            rel2 = sylvan_or(rel2, temp1);
        }
        temp1 = temp2;
        for(int copy=j-1;copy>=0;copy--){ //west unchanged 
            temp1 = s_and(temp1, s_or(sylvan_and(F[i][copy][E][0], F[i][copy][E][1]), s_or(sylvan_and(F[i][copy][B][0], F[i][copy][B][1]), sylvan_and(F[i][copy][W][0], F[i][copy][W][1]))));
        }
        for(int west=j-2;west>=0;west--){ // all enclosers west of fieldToPlay must be removed from temp1
            for(int walker=j-1;walker>west;walker--){
                reduceUnchanged = sylvan_and(reduceUnchanged, F[i][walker][!p][0]);
            }
            reduceUnchanged = sylvan_and(reduceUnchanged, F[i][west][p][0]);
            temp1 = sylvan_diff(temp1, reduceUnchanged);
            reduceUnchanged = sylvan_true;
        }
        toRemove = sylvan_and(toRemove, temp1);
        rel2 = sylvan_or(rel2, temp1);
        rel = sylvan_and(rel,rel2);
        rel2 = sylvan_false;

        //ENCLOSE TO NORTH
        for(int north=i-2;north>=0;north--){ // all enclosers north of fieldToPlay
            temp1 = s_and(temp2, one_direction_enclose(p,i,j,north,j));
            rel2 = sylvan_or(rel2, temp1);
        }
        temp1 = temp2;
        for(int copy=i-1;copy>=0;copy--){ //north unchanged
            temp1 = s_and(temp1, s_or(sylvan_and(F[copy][j][E][0], F[copy][j][E][1]), s_or(sylvan_and(F[copy][j][B][0], F[copy][j][B][1]), sylvan_and(F[copy][j][W][0], F[copy][j][W][1]))));
        }
        for(int north=i-2;north>=0;north--){ // all enclosers south of fieldToPlay must be removed from temp1
            for(int walker=i-1;walker>north;walker--){
                reduceUnchanged = sylvan_and(reduceUnchanged, F[walker][j][!p][0]);
            }
            reduceUnchanged = sylvan_and(reduceUnchanged, F[north][j][p][0]);
            temp1 = sylvan_diff(temp1, reduceUnchanged);
            reduceUnchanged = sylvan_true;
        }
        toRemove = sylvan_and(toRemove, temp1);
        rel2 = sylvan_or(rel2, temp1);
        rel = sylvan_and(rel,rel2);
        rel2 = sylvan_false;

        int south, east, north, west, copyi, copyj;
        //ENCLOSE TO SOUTHEAST
        south = i+2;
        east = j+2;
        while(south<L && east<Wi){ // all enclosers southeast of fieldToPlay
            temp1 = s_and(temp2, one_direction_enclose(p,i,j,south,east));
            rel2 = sylvan_or(rel2, temp1);
            south++;
            east++;
        }
        copyi = i;
        copyj = j;
        temp1 = temp2;
        while(copyi<L-1 && copyj<Wi-1){ //southeast unchanged
            copyi++;
            copyj++;
            temp1 = s_and(temp1, s_or(sylvan_and(F[copyi][copyj][E][0], F[copyi][copyj][E][1]), s_or(sylvan_and(F[copyi][copyj][B][0], F[copyi][copyj][B][1]), sylvan_and(F[copyi][copyj][W][0], F[copyi][copyj][W][1]))));
        }
        south = i+2;
        east = j+2;
        while(south<L && east<Wi){ // all enclosers southeast of fieldToPlay must be removed from temp1
            int count = 0; // count iteration below
            for(int walker=j+1;walker<east;walker++){
                count++;
                reduceUnchanged = sylvan_and(reduceUnchanged, F[i+count][walker][!p][0]);
            }
            reduceUnchanged = sylvan_and(reduceUnchanged, F[south][east][p][0]);
            temp1 = sylvan_diff(temp1, reduceUnchanged);
            reduceUnchanged = sylvan_true;
            south++;
            east++;
        }
        toRemove = sylvan_and(toRemove, temp1);
        rel2 = sylvan_or(rel2, temp1);
        rel = sylvan_and(rel,rel2);
        rel2 = sylvan_false;

        //ENCLOSE TO SOUTHWEST
        south = i+2;
        west = j-2;
        while(south<L && west>=0){ // all enclosers southwest of fieldToPlay
            temp1 = s_and(temp2, one_direction_enclose(p,i,j,south,west));
            rel2 = sylvan_or(rel2, temp1);
            south++;
            west--;
        }
        copyi = i;
        copyj = j;
        temp1 = temp2;
        while(copyi<L-1 && copyj>0){ //southwest unchanged
            copyi++;
            copyj--;
            temp1 = s_and(temp1, s_or(sylvan_and(F[copyi][copyj][E][0], F[copyi][copyj][E][1]), s_or(sylvan_and(F[copyi][copyj][B][0], F[copyi][copyj][B][1]), sylvan_and(F[copyi][copyj][W][0], F[copyi][copyj][W][1]))));
        }
        south = i+2;
        west = j-2;
        while(south<L && west>=0){ // all enclosers southwest of fieldToPlay must be removed from temp1
            int count = 0; // count iteration below
            for(int walker=j-1;walker>west;walker--){
                count++;
                reduceUnchanged = sylvan_and(reduceUnchanged, F[i+count][walker][!p][0]);
            }
            reduceUnchanged = sylvan_and(reduceUnchanged, F[south][west][p][0]);
            temp1 = sylvan_diff(temp1, reduceUnchanged);
            reduceUnchanged = sylvan_true;
            south++;
            west--;
        }
        toRemove = sylvan_and(toRemove, temp1);
        rel2 = sylvan_or(rel2, temp1);
        rel = sylvan_and(rel,rel2);
        rel2 = sylvan_false;

        //ENCLOSE TO NORTHWEST
        north = i-2;
        west = j-2;
        while(north>=0 && west>=0){ // all enclosers northwest of fieldToPlay
            temp1 = s_and(temp2, one_direction_enclose(p,i,j,north,west));
            rel2 = sylvan_or(rel2, temp1);
            north--;
            west--;
        }
        copyi = i;
        copyj = j;
        temp1 = temp2;
        while(copyi>0 && copyj>0){ //northwest unchanged
            copyi--;
            copyj--;
            temp1 = s_and(temp1, s_or(sylvan_and(F[copyi][copyj][E][0], F[copyi][copyj][E][1]), s_or(sylvan_and(F[copyi][copyj][B][0], F[copyi][copyj][B][1]), sylvan_and(F[copyi][copyj][W][0], F[copyi][copyj][W][1]))));
        }
        north = i-2;
        west = j-2;
        while(north>=0 && west>=0){ // all enclosers northwest of fieldToPlay must be removed from temp1
            int count = 0; // count iteration below
            for(int walker=j-1;walker>west;walker--){
                count++;
                reduceUnchanged = sylvan_and(reduceUnchanged, F[i-count][walker][!p][0]);
            }
            reduceUnchanged = sylvan_and(reduceUnchanged, F[north][west][p][0]);
            temp1 = sylvan_diff(temp1, reduceUnchanged);
            reduceUnchanged = sylvan_true;
            north--;
            west--;
        }
        toRemove = sylvan_and(toRemove, temp1);
        rel2 = sylvan_or(rel2, temp1);
        rel = sylvan_and(rel,rel2);
        rel2 = sylvan_false;

        //ENCLOSE TO NORTHEAST
        north = i-2;
        east = j+2;
        while(north>=0 && east<Wi){ // all enclosers northeast of fieldToPlay
            temp1 = s_and(temp2, one_direction_enclose(p,i,j,north,east));
            rel2 = sylvan_or(rel2, temp1);
            north--;
            east++;
        }
        copyi = i;
        copyj = j;
        temp1 = sylvan_true;
        while(copyi>0 && copyj<Wi-1){ //northeast unchanged
            copyi--;
            copyj++;
            temp1 = s_and(temp1, s_or(sylvan_and(F[copyi][copyj][E][0], F[copyi][copyj][E][1]), s_or(sylvan_and(F[copyi][copyj][B][0], F[copyi][copyj][B][1]), sylvan_and(F[copyi][copyj][W][0], F[copyi][copyj][W][1]))));
        }
        north = i-2;
        east = j+2;
        while(north>=0 && east<Wi){ // all enclosers northeast of fieldToPlay must be removed from temp1
            int count = 0; // count iteration below
            for(int walker=j+1;walker<east;walker++){
                count++;
                reduceUnchanged = sylvan_and(reduceUnchanged, F[i-count][walker][!p][0]);
            }
            reduceUnchanged = sylvan_and(reduceUnchanged, F[north][east][p][0]);
            temp1 = sylvan_diff(temp1, reduceUnchanged);
            reduceUnchanged = sylvan_true;
            north--;
            east++;
        }
        toRemove = sylvan_and(toRemove, temp1);
        rel2 = sylvan_or(rel2, temp1);
        rel = sylvan_and(rel,rel2);
        rel2 = sylvan_false;
        rel = sylvan_diff(rel, toRemove); // remove from rel where in all directions unchanged
        if(p==0){
            relFirstPlayer = rel;
        }
        else{
            rel = sylvan_or(rel, relFirstPlayer);
        }
        toRemove = sylvan_true; // So that in the second iteration of the loop toRemove wont contain current iteration..
    }
    //takes care of: all fields Vij that are not in partial rel of this field, must be unchanged.
    vector<int>::iterator it;
    int v=0;
    for(int x=0;x<L;x++){
        for(int y=0;y<Wi;y++){
            it = find(varsArray.begin(), varsArray.end(), Wi*x+y);
            v = distance(varsArray.begin(), it)*4+4;
            if(!sylvan_set_in(sylvan_support(rel),v)){
                rel = s_and(rel, s_or(sylvan_and(F[x][y][E][0], F[x][y][E][1]), s_or(sylvan_and(F[x][y][B][0], F[x][y][B][1]), sylvan_and(F[x][y][W][0], F[x][y][W][1]))));
            }
        }        
    }
    cout << "number of nodes " << sylvan_nodecount(rel) << ", sat count: "<< sylvan_satcount(rel,VR0) << endl;
    bdd_refs_popptr(7);
    return rel;
}

// generate transition relations and all winning positions for player 0 (white)
void rel_and_winning_positions(BDD *rel){
    LACE_ME;
    BDD winning = sylvan_false;
    bdd_refs_pushptr(&winning);
    BDD fullrel = sylvan_false;
    bdd_refs_pushptr(&fullrel);
    BDD deadlocks = sylvan_true;
    bdd_refs_pushptr(&deadlocks);
    rel[DL] = sylvan_true;

    for(int i=0;i<L;i++){
        for(int j=0;j<Wi;j++){
            rel[Wi*i+j] = init_othello_transition_relation(i, j);
            fullrel = sylvan_or(fullrel, rel[Wi*i+j]);
            rel[DL] =   s_and(rel[DL],
                        s_or(sylvan_and(F[i][j][E][0], F[i][j][E][1]),
                        s_or(sylvan_and(F[i][j][B][0], F[i][j][B][1]),
                        sylvan_and(F[i][j][W][0], F[i][j][W][1]))));
        }
        cout << endl;
    }
    //bdd_refs_popptr(5);
    //return sylvan_false;

    rel[DL] = s_and(rel[DL], s_or(sylvan_and(P[0][0], P[1][1]), sylvan_and(P[1][0], P[0][1])));
    cout << "deadlocks berekenen..." << endl;
    for(int i=0;i<L*Wi;i++){
        deadlocks = s_and(deadlocks, sylvan_forall_preimage(sylvan_false, rel[i]));
    }
    cout << "number of nodes in deadlocks " << sylvan_nodecount(deadlocks) << ", sat count: "<< sylvan_satcount(deadlocks,VA0) << endl;

    BDD end = sylvan_and(sylvan_low(deadlocks), sylvan_high(deadlocks)); //only works if turn vars on top
    bdd_refs_pushptr(&end);
    cout << "number of nodes in end " << sylvan_nodecount(end) << ", sat count: "<< sylvan_satcount(end, VA0) << endl;

    deadlocks = sylvan_diff(deadlocks, end); //only one player cannot move (player with turn)
    rel[DL] = s_and(rel[DL], deadlocks);
    cout << "number of nodes in rel-skip " << sylvan_nodecount(rel[DL]) << ", sat count: "<< sylvan_satcount(rel[DL],VR0) << endl;

    fullrel = s_or(fullrel, rel[DL]);

    cout << "number of nodes in relation " << sylvan_nodecount(fullrel) << ", sat count: "<< sylvan_satcount(fullrel,VR0) << endl;
    //fullrel = sylvan_diff(fullrel, end);
    //cout << "number of nodes in relation pruned " << sylvan_nodecount(fullrel) << ", sat count: "<< sylvan_satcount(fullrel,VR0) << endl;

    rel[DL+1] = fullrel;

    winning = end; //both players can't move, so exactly the set of finished boards
    winning = s_and(winning, moreWhiteStones());
    rel[DL+2] = winning;
    cout << "number of nodes in winning " << sylvan_nodecount(winning) << ", sat count: "<< sylvan_satcount(winning,VA0) << endl;
    bdd_refs_popptr(4);
}

BDD reachable(BDD start, BDD *rel){
    LACE_ME;
    BDD move = sylvan_false;
    bdd_refs_pushptr(&move);
    start = s_or(start, sylvan_relnext(start, rel[DL], VR[DL])); //base case, will never happen with normal start boards
    //calculate reachable boards with i stones or less
    for(int i=5;i<=L*Wi;i++){
        cout << "reachability iteration: " << i << endl;
        for (int j=L*Wi-1; j>=0; j--) {
            move = s_or(move, sylvan_relnext(start, rel[j], VR[j]));//add all boards to move with one more stone
        }
        move = s_or(move, sylvan_relnext(move, rel[DL], VR[DL])); //add all boards also with i stones by using DL relation
        start = move; // set of reachable boards with i stones or less
    }
    bdd_refs_popptr(1);
    return move;
}

void superSaturationF(int i, BDD &move, BDD *rel){
    LACE_ME;
    if(i==0){
        move = s_or(move, sylvan_relnext(move, rel[varsArray[i]], VR[varsArray[i]]));
        //cout << "last concatenated, nodes =" << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
        return;
    }
    BDD copymove = sylvan_false;
    bdd_refs_pushptr(&copymove);
    while(copymove!=move){
        copymove = move;
        superSaturationF(i-1, move, rel);
    }
    bdd_refs_popptr(1);
    move = s_or(move, sylvan_relnext(move, rel[varsArray[i]], VR[varsArray[i]]));
    //cout << "at i: " << i << ", nodes =" << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
    return;

    /* valid for testing
    LACE_ME;
    //if(i<20){cout << "i: " << i << endl;}
    if(i==6){
        if(i<23){cout << "concatenate i: " << i << endl;}
        return s_or(move, sylvan_relnext(move, rel[i], VR[i]));
    }
    BDD copymove = sylvan_false;
    bdd_refs_pushptr(&copymove);
    for(int p=0;p<3;p++){
        copymove = move;
        move = superSaturation(i+1, move, rel);
    }
    bdd_refs_popptr(1);
    //cout << "for the coming conc, i = " << i << endl;
    move = s_or(move, sylvan_relnext(move, rel[i], VR[i]));
    cout << "concatenate i: " << i << "and i = " << i << endl;
    return move;*/
}

void superSaturationB(int i, BDD &move, BDD *rel){
    LACE_ME;
    if(i==0){
        move = s_or(move, sylvan_relprev(rel[varsArray[i]], move, VR[varsArray[i]]));
        //cout << "last concatenated, nodes =" << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
        return;
    }
    BDD copymove = sylvan_false;
    bdd_refs_pushptr(&copymove);
    while(copymove!=move){
        copymove = move;
        superSaturationB(i-1, move, rel);
    }
    bdd_refs_popptr(1);
    move = s_or(move, sylvan_relprev(rel[varsArray[i]], move, VR[varsArray[i]]));
    //cout << "at i: " << i << ", nodes =" << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
    return;
}

class MyNumPunct : public numpunct<char>
{
protected:
    virtual char_type do_decimal_point() const      { return ',';   }
    virtual char do_thousands_sep() const           { return '.';   }
    virtual string do_grouping() const              { return "\03"; }
};

//arguments:
//1: algorithm
//2: location of upperleft field of start square
int main(int argc, const char *argv[]) { 

    cout.imbue(locale( locale::classic(), new MyNumPunct ) );

    if(argc!=3){
        cout << "Wrong amount of arguments passed.. expected 2 arguments." << endl;
        exit(EXIT_FAILURE);
    }
    int alg = atoi(argv[1]);
    cout << "Algorithm " << alg << " will be used." << endl;
    int odd_place = atoi(argv[2]);
    if(odd_place%Wi==Wi-1){
        cout << "Starting positions too far east." << endl;
        exit(EXIT_FAILURE);
    }
    if((int)floor(odd_place/Wi)>=L-1){
        cout << "Starting positions too far south." << endl;
        exit(EXIT_FAILURE);
    }

    int n_workers = 0; // auto-detect
    lace_init(n_workers, 0);
    lace_startup(0, NULL, NULL);
    LACE_ME;

    // use at most 512 MB, nodes:cache ratio 2:1, initial size 1/32 of maximum
        // Init Sylvan
    // Nodes table size of 1LL<<20 is 1048576 entries
    // Cache size of 1LL<<18 is 262144 entries
    // Nodes table size: 24 bytes * nodes
    // Cache table size: 36 bytes * cache entries
    // With 2^20 nodes and 2^18 cache entries, that's 33 MB
    // With 2^24 nodes and 2^22 cache entries, that's 528 MB
    //sylvan_set_sizes(1ULL<<25, 1ULL<<36, 1ULL<<23, 1ULL<<34);
    //sylvan_set_sizes(1ULL<<25, 1ULL<<32, 1ULL<<23, 1ULL<<30);
    //sylvan_set_sizes(1LL<<26, 1LL<<29, 1LL<<23, 1LL<<25); // previous, worked ok
    sylvan_set_sizes(1LL<<26, 1LL<<31, 1LL<<23, 1LL<<25); //faster, works

    //sylvan_set_sizes(1LL<<25, 1LL<<27, 1LL<<24, 1LL<<26);
    sylvan_init_package();
    sylvan_init_bdd();
    /* ... do stuff ... */
    sylvan_gc_hook_pregc(TASK(gc_start));
    sylvan_gc_hook_postgc(TASK(gc_end));
    sylvan_gc_disable();

    calcVarOrder();
//    for(size_t i=0; i<varsArray.size(); ++i){
//        cout << varsArray[i] << ' ';
//    }
    init_othello();
    BDD start = starting_position(1, odd_place); //blacks starts
    sylvan_protect(&start);
    cout << "number of nodes in start board " << sylvan_nodecount(start) << endl;
    //CALL(sylvan_enum, start, VA0, printboard, NULL);

    /*uint32_t array[sylvan_set_count(VR[0])];
    sylvan_set_toarray(VR[0], array);
    for(size_t i=0;i<sylvan_set_count(VR[0]);i++){
        cout << array[i] << endl;
    }*/  

    //calculate the relation
    static BDD rel[L*Wi + 3] = {sylvan_false};
    for(int i = 0; i<L*Wi+3; i++) {
        rel[i] = sylvan_false;
        sylvan_protect(&rel[i]);
    }
    BDD winning = sylvan_false;
    sylvan_protect(&winning);
    BDD fullrel = sylvan_false;
    sylvan_protect(&fullrel);

    if (1) {
        printf ("Creating Othello transition relation.\n");
        //calculate winning set
        rel_and_winning_positions(rel); // find all winning positions for player 0 (white)

//        printf ("Writing Awari transition relation to othello-rel.\n");
//        FILE *f = fopen("othello-rel.bdd", "w");
//        assert (f != NULL && "cannot open file");
//        mtbdd_writer_tobinary(f, rel, DL+3);
//        fclose(f);
//        exit(0);
    } else {
        FILE *f = fopen("othello-rel.bdd", "r");
        assert (f != NULL && "cannot open file");
        int res = mtbdd_reader_frombinary(f, rel, DL+3);
        assert (!res);
        fclose(f);
    }
    fullrel = rel[DL+1];
    winning = rel[DL+2];

    //CALL(sylvan_enum, winning, VA0, printboard, NULL);

    BDD move = sylvan_false;
    sylvan_protect(&move);
    BDD line2 = sylvan_false;
    sylvan_protect(&line2);
    BDD tmp = sylvan_false;
    sylvan_protect(&tmp);
    BDD tmp2 = sylvan_false;
    sylvan_protect(&tmp2);
    BDD tmp3 = sylvan_false;
    sylvan_protect(&tmp3);
    BDD copymove = sylvan_false;
    sylvan_protect(&copymove);
    int loops = 0;
    auto startTime = chrono::system_clock::now();
    auto endTime = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds;
    ofstream outfile;
    //outfile.imbue(locale( locale::classic(), new MyNumPunct ) );
    /*FILE * fileToWrite; //printing dot BDD
    fileToWrite = fopen("printBDD.txt", "w");
    sylvan_fprintdot(fileToWrite, sylvan_or(sylvan_or(start, sylvan_relnext(start, rel[14], VR[14])), sylvan_relnext(start, rel[11], VR[11])));*/

    if (alg==0){//BFS forward
        cout << "Now running forward" << endl;
        outfile.open("sizesforward.txt", ofstream::app);
        loops = 0;
        move = start;
        cout << "Level ["<< setw(2) << loops << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
            <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
        outfile << loops << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        while (copymove!=move){
            copymove = move;

//            move = s_or(move, sylvan_relnext(start, fullrel, VR0));
            for (int i=L*Wi; i>=0; i--) {
                move = s_or(move, sylvan_relnext(start, rel[i], VR[i]));
            }
            start = move;
            cout << "Level ["<< setw(2) << loops+1 << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
                 <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
            outfile << loops+1 << "\t" << setw(13) << sylvan_nodecount(move) << endl;
            loops++;
        }
        outfile.close();
    }

    else if(alg==1){//BFS backward
        cout << "Now running backward" << endl;
        outfile.open("sizesbackward.txt", ofstream::app);
        int loops = 0;
        move = winning;
        cout << "Level ["<< setw(2) << loops << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
            <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
        outfile << loops << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        while(copymove!=move){
            //cout << "loop number: " << loops << endl;
            copymove=move;
            //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;

            //add al white moves going to winning
            tmp = sylvan_and(winning, P[1][0]); //want to find all moves for white, so in winning it must be blacks turn unprimed.
            move = s_or(move, sylvan_relprev(fullrel, tmp, VR0));//add al white moves going to winning

            //add all black moves going to winning
            winning = sylvan_and(move, P[0][0]); //winning = (winning+all white moves) AND white must have turn, s.t. in the prev black has turn.
            tmp = sylvan_relprev(fullrel, winning, VR0);
            tmp2 = sylvan_not(winning);
            tmp2 = sylvan_relprev(fullrel, tmp2, VR0);
            move = s_or(move, sylvan_diff(tmp, tmp2));

            winning = move;
            cout << "Level ["<< setw(2) << loops+1 << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
                <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
            outfile << loops+1 << "\t" << setw(13) << sylvan_nodecount(move) << endl;
            loops++;
        }
        move = sylvan_and(move, start);
        //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
        if(move !=sylvan_false){
            cout << "Dit spel is winnend voor wit (zwart begint)!" << endl;
            //CALL(sylvan_enum, move, VA0, printboard, NULL);
        }
        else{
            cout << "Dit spel is winnend voor zwart of gelijkspel (zwart begint)!" << endl;
        }
        outfile.close();
    }

    //TODO intersect with reachable in every iteration!
    if(alg==8){//backward limited to reachables states (first calculate reachable states, then do backward) 
        cout << "Now running backward + reachable" << endl;
        outfile.open("sizesbackward_reachable.txt", ofstream::app);
        winning = s_and(winning, reachable(start, rel));

        int loops = 0;
        move = winning;
        cout << "Level ["<< setw(2) << loops << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
            <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
        outfile << loops << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        while(copymove!=move){
            //cout << "loop number: " << loops << endl;
            copymove=move;
            //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
            //add all white moves going to winning
            tmp = sylvan_and(winning, P[1][0]); //want to find all moves for white, so in winning it must be blacks turn unprimed.
            move = s_or(move, sylvan_relprev(fullrel, tmp, VR0));//add al white moves going to winning
            
            //add all black moves going to winning
            winning = sylvan_and(move, P[0][0]); //winning = (winning+all white moves) AND white must have turn, s.t. in the prev black has turn.
            tmp = sylvan_relprev(fullrel, winning, VR0);
            tmp2 = sylvan_not(winning);
            tmp2 = sylvan_relprev(fullrel, tmp2, VR0);
            move = s_or(move, sylvan_diff(tmp, tmp2));
            //move = s_or(move, sylvan_forall_preimage(winning, fullrel));
            //tmp = sylvan_forall_preimage(sylvan_false, fullrel);
            //move = sylvan_diff
            winning = move;
            cout << "Level ["<< setw(2) << loops+1 << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
            <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
            outfile << loops+1 << "\t" << setw(13) << sylvan_nodecount(move) << endl;
            loops++;
        }
        move = sylvan_and(move, start);
        //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
        if(move !=sylvan_false){
            cout << "Dit spel is winnend voor wit (zwart begint)!" << endl;
            //CALL(sylvan_enum, move, VA0, printboard, NULL);
        }
        else{
            cout << "Dit spel is winnend voor zwart of gelijkspel (zwart begint)!" << endl;
        }
        outfile.close();
    }

    else if(alg==9){//forward + sweepline
        cout << "Now running forward + sweepline" << endl;
        outfile.open("sizesforward_sweepline.txt", ofstream::app);
        move = start;
        cout << "Level ["<< setw(2) << 0 << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
            <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
        outfile << 0 << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        for(int i=5;i<=L*Wi;i++){//number of stones
            move = sylvan_false;
            for (int j=L*Wi-1; j>=0; j--) {
                move = s_or(move, sylvan_relnext(start, rel[j], VR[j]));//add all boards with i stones
            }
            move = s_or(move, sylvan_relnext(move, rel[DL], VR[DL])); //add all boards also with i stones by using DL relation
            start = move;
            cout << "Level ["<< setw(2) << i-4 << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
                 <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
            outfile << i-4 << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        }
        outfile.close();
    }

    else if(alg==10){//backward + sweepline, note that sweepline only works because max. 1 stone is added. If 2 stones could be added, preimage would do fine but forallpreimage not. 
        //for example when there exists a state with 2 successors, one in E_0 and one added later to X. Then it will never be added to X, but it should be.
        cout << "Now running backward + sweepline" << endl;
        outfile.open("sizesbackward_sweepline.txt", ofstream::app);
        move = sylvan_and(winning, boardWithXStones(L*Wi)); //Line1
        cout << "Level ["<< setw(2) << 0 << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
            <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
        outfile << 0 << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        for(int i=L*Wi-1;i>=4;i--){//number of stones
            //add al white moves going to move(line1)
            tmp = sylvan_and(move, P[1][0]); //want to find all moves for white, so in winning it must be blacks turn unprimed.
            line2 = sylvan_false;
            for (int j=L*Wi-1; j>=0; j--) {
                line2 = s_or(line2, sylvan_relprev(rel[j], tmp, VR0));//add all white moves to i stones
            }
            //add all black moves going to move(line1)
            tmp = sylvan_and(move, P[0][0]); //want to find all moves for black, so in winning it must be whites turn unprimed.
            tmp2 = sylvan_relprev(fullrel, tmp, VR0);
            tmp3 = sylvan_not(tmp);
            tmp3 = sylvan_relprev(fullrel, tmp3, VR0);
            line2 = s_or(line2, sylvan_diff(tmp2, tmp3));

            //add all boards with also i stones by DL relation
            line2 = s_or(line2, sylvan_relprev(rel[DL], line2, VR[DL])); 
            move = s_or(line2, sylvan_and(winning, boardWithXStones(i)));
            cout << "Level ["<< setw(2) << L*Wi-i << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
                <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
            outfile << L*Wi-i << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        }
        move = sylvan_and(move, start);
        //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
        if(move !=sylvan_false){
            cout << "Dit spel is winnend voor wit (zwart begint)!" << endl;
            //CALL(sylvan_enum, move, VA0, printboard, NULL);
        }
        else{
            cout << "Dit spel is winnend voor zwart of gelijkspel (zwart begint)!" << endl;
        }
        outfile.close();
    }

    else if(alg==11){//backward limited to reachable states + sweepline
        cout << "Now running backward + sweepline + reachable" << endl;
        outfile.open("sizesbackward_reachable_sweepline.txt", ofstream::app);
        winning = s_and(winning, reachable(start, rel));
        move = sylvan_and(winning, boardWithXStones(L*Wi)); //Line1
        cout << "Level ["<< setw(2) << 0 << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
            <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
        outfile << 0 << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        move = sylvan_and(winning, boardWithXStones(L*Wi));
        for(int i=L*Wi-1;i>=4;i--){//number of stones
            //add al white moves going to winning
            tmp = sylvan_and(move, P[1][0]); //want to find all moves for white, so in winning it must be blacks turn unprimed.
            line2 = sylvan_false;
            for (int j=L*Wi-1; j>=0; j--) {
                line2 = s_or(line2, sylvan_relprev(rel[j], tmp, VR0));//add all white moves to i-1 stones
            }

            //add all black moves going to winning
            tmp = sylvan_and(move, P[0][0]); //want to find all moves for black, so in winning it must be whites turn unprimed.
            tmp2 = sylvan_relprev(fullrel, tmp, VR0);
            tmp3 = sylvan_not(tmp);
            tmp3 = sylvan_relprev(fullrel, tmp3, VR0);
            line2 = s_or(line2, sylvan_diff(tmp2, tmp3));

            //add all boards with also i-1 stones by DL relation
            line2 = s_or(line2, sylvan_relprev(rel[DL], line2, VR[DL])); 

            move = s_or(line2, sylvan_and(winning, boardWithXStones(i)));
            cout << "Level ["<< setw(2) << L*Wi-i << "] number of boards: "<< setw(13) << (size_t)sylvan_satcount(move,VA0)
                <<" [nodes: "<< setw(13) << sylvan_nodecount(move) << "]"<< endl;
            outfile << L*Wi-i << "\t" << setw(13) << sylvan_nodecount(move) << endl;
        }
        move = sylvan_and(move, start);
        //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
        if(move !=sylvan_false){
            cout << "Dit spel is winnend voor wit (zwart begint)!" << endl;
            //CALL(sylvan_enum, move, VA0, printboard, NULL);
        }
        else{
            cout << "Dit spel is winnend voor zwart of gelijkspel (zwart begint)!" << endl;
        }
        outfile.close();
    }

    //CHAINING forward
    else if(alg==2){
        loops = 0;
        move = start;
        while(copymove!=move){
            //cout << "loop number: " << loops << endl;
            copymove=move;
            for(int i=L*Wi-1;i>=0;i--){ 
                //cout << "i: " << i << endl;
                move = s_or(move, sylvan_relnext(move, rel[varsArray[i]], VR[varsArray[i]]));
                endTime = chrono::system_clock::now();
                elapsed_seconds = endTime-startTime;
                outfile << elapsed_seconds.count() << "\t" << sylvan_nodecount(move) << endl;
                //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
                //cout << "beforemove " << i << ": " << beforeMove << endl;
                
            }
            loops++;
        }
    }

    //CHAINING backward
    else if(alg==3){
        loops = 0;
        move = winning;
        while(copymove!=move){
            cout << "loop number: " << loops << endl; 
            copymove=move;
            for(int i=L*Wi-1;i>=0;i--){ 
                /*if(i==L*Wi/2+Wi/2 ||i==L*Wi/2+Wi/2-1 || i==L*Wi/2+Wi/2-Wi ||i==L*Wi/2+Wi/2-Wi-1){ //when no moves in middle squares possible(backward)
                    //cout << "ill continue at: " << i << endl;
                    continue;
                }*/ //aanpassen naar juiste middle square (is nu variabel)
                cout << "i: " << i << endl;
                tmp = sylvan_and(move, P[1][0]); //want to find all moves for white, so in totalset it must be blacks turn unprimed.
                move = s_or(move, sylvan_relprev(rel[varsArray[i]], tmp, VR[varsArray[i]]));
                tmp = sylvan_and(move, P[0][0]);
                tmp = s_and(tmp, sylvan_forall_preimage(move, rel[varsArray[i]]));
                /*endTime = chrono::system_clock::now();
                elapsed_seconds = endTime-startTime;
                outfile << elapsed_seconds.count() << "\t" << sylvan_nodecount(move) << endl;*/
                //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;       
            }
            loops++;
        }
    }

    //SATURATION forward
    else if(alg==4){
        loops = 0;
        move = start;
        for(int i=L*Wi-1;i>=0;i--){
            copymove = sylvan_false;
            loops = 0;
            while(copymove != move){
                //cout << "amount of loops: " << loops << endl;
                //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;                
                copymove = move;
                for(int j=L*Wi-1;j>=i;j--){
                    //cout << " i: " << i << " j: " << j << endl;
                    move = s_or(move, sylvan_relnext(move, rel[varsArray[i]], VR[varsArray[i]]));
                    endTime = chrono::system_clock::now();
                    elapsed_seconds = endTime-startTime;
                    outfile << elapsed_seconds.count() << "\t" << sylvan_nodecount(move) << endl;   
                }
                loops++;
            }
        }
    }

    //SATURATION backward
    if(alg==5){
        loops = 0;
        move = winning;
        for(int i=L*Wi-1;i>=0;i--){
            copymove = sylvan_false;
            loops = 0;
            while(copymove != move){
                //cout << "amount of loops: " << loops << endl;
                //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;                
                copymove = move;
                for(int j=L*Wi-1;j>=i;j--){
                    /*if(i==L*Wi/2+Wi/2 ||i==L*Wi/2+Wi/2-1 || i==L*Wi/2+Wi/2-Wi ||i==L*Wi/2+Wi/2-Wi-1){//when no moves in middle squares possible(backward)
                        cout << "ill continue at: " << j << endl;
                        continue;
                    }*/
                    //cout << " i: " << i << " j: " << j << endl;
                    move = s_or(move, sylvan_relprev(rel[varsArray[i]], move, VR[varsArray[i]])); 
                    endTime = chrono::system_clock::now();
                    elapsed_seconds = endTime-startTime;
                    outfile << elapsed_seconds.count() << "\t" << sylvan_nodecount(move) << endl;  
                }
                loops++;
            }
        }
    }

    //superSATURATION forward
    if(alg==6){
        move = start;
        copymove = sylvan_false;
        while(copymove!=move){
            copymove = move;
            superSaturationF(L*Wi-1, move, rel);
            //cout << "one loop done.. with all fields" << endl;
        }
        //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
    }

    //superSATURATION backward
    if(alg==7){
        move = winning;
        copymove = sylvan_false;
        while(copymove!=move){
            copymove = move;
            superSaturationB(L*Wi-1, move, rel);
            //cout << "one loop done.. with all fields" << endl;
        }
        //cout << "number of nodes " << sylvan_nodecount(move) << ", sat count: "<< sylvan_satcount(move,VA0) << endl;
    }

   //CALL(sylvan_enum, winning, VA0, printboard, NULL);
    endTime = chrono::system_clock::now();
    elapsed_seconds = endTime-startTime;
    cout << "Elapsed time: " << elapsed_seconds.count() << "s" << endl;
    //outfile << elapsed_seconds.count() << endl;
    outfile.close();
    exit(0);  // (faster than "return", which will invoke the destructors)
    //delete cnf;
}
