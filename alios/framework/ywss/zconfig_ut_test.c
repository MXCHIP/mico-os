/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifdef UT_TEST

#include <string.h>
#include <stdio.h>
#include "sha2.h"
#include "zconfig_utils.h"

#define test_case(model, secret, tpsk)  #model, #secret, #tpsk,

//model_secret_tpsk_table
char *mst_table[] = {
    //xiongmai
    test_case(XIONGMAI_SECURITY_IPCAMERA_TEST, KEz3d6owcQwevwnNx7CGqe5ohLra8kGGXW1PrDvJ, KD7ziRXDE + yMKHYXO3V2jTAEb1vya1ZwEM4XaSR4SQY = )
    test_case(XIONGMAI_SECURITY_IPCAMERA_PTZ, 8lLbGlsNDy1YjeojOpvvvcpr0H5lnhN1IzCZjGZU, WUbJvEkVsIrJNVMtHCXsTERcUpgvp0lCLcsIW2daHVw = )
    test_case(XIONGMAI_SECURITY_IPCAMERA_FIXED, 5pEscD4mtJCEPSg2oqd2hkb191wyOLMPykMN4JI4, KSXBIkrOBJ2vzlcEsj1SIUPzr4dqvbO + m82LMs07Hqs = )
    test_case(XIONGMAI_SECURITY_IPCAMERA_960PTZ, LoGB4MqErRYoiLONA8lJOayFVm7OpvN0o16zPtMQ, g0o / BykCli8mL0wAy1DwkHbdujj6XBbMevw7dlwYZus = )

    //YOYON
    test_case(YOYON_SECURITY_SMARTDOOR_YOYON_ALOCK_GATEWAY, UgHzTF4vujoUMUp4aM2USLq3cDPb6YctZed3MmV9, Kvk2oURUlYMqedMZ2s50aAMGBhksBeNjd9ARAfdnsv8 = )

    //YOYON product
    test_case(YOYON_SECURITY_GATEWAY_ALOCK_GATEWAY, aQ946rsNThRO592gUOz4eOtuBzmTyxsm54dbT34j, 9OAiNj + 3Vb5HJkvXOw6a + 4668okBD6prSDBOALpcvSo = )

    //公牛
    test_case(BULL_LIVING_OUTLET_GN_WIFI_Y2010, 124X0LZdisxkUiJvfwcce8HshwuioW0GmDeEY3Ho, fSBOmkbVyebXC3h0bQSLoO4qBmskkABWwvtZspM1lfk = )

    //GIBSION
    test_case(GIBSON_ENTERTAINMENT_ATALK_AW6005A, FQlWkhLnCEdTGjZC6hvQgQry4XGh022EW7QZLFak, c4F + 0rbvfs55GC1rt / 8 / pKMGIoSO4i4dNONksniuGzU = )

    //SMARTLED LUA
    test_case(ALINKTEST_LIVING_LIGHT_SMARTLED_LUA, W6tXrtzgQHGZqksvJLMdCPArmkecBAdcr2F5tjuF, Pu99UdYLaxIzIdcF6kE69IY7KamoBFef2lVSq5wkGBc = )

    //SMARTLED
    test_case(ALINKTEST_LIVING_LIGHT_SMARTLED, YJJZjytOCXDhtQqip4EjWbhR95zTgI92RVjzjyZF, A59Vz + m6owaeTVPO9yJYN9YZBBw / 924 + A3CxO / CAXGM = )

    //junzheng
    test_case(WONDERS_ENTERTAINMENT_ATALK_DS1825, 8j3zDvtFpqTVxH0wHw7mEBv2WTuR3EVLzNu6HsZN, 32mlYwJKrfabBMzSjbo + tACBj + VADZZDxHnDFfo0mto = )

    NULL, NULL, NULL
};

//ut_test for zconfig_calc_tpsk()
void ut_test(void)
{
    int i;
    char tpsk[64], *res;

    for (i = 0; mst_table[i]; i += 3) {
        memset(&tpsk[0], 0, sizeof(tpsk));
        res = zconfig_calc_tpsk(mst_table[i], mst_table[i + 1], tpsk);

        if (!strcmp(mst_table[i + 2], res)) {
            printf("%d: pass\n", i / 3);
        } else {
            printf("%d: fail\n", i / 3);
            printf("%s, %s, %s, res: %s\n", mst_table[i], mst_table[i + 1], mst_table[i + 2], res);
        }
    }
}

#endif
