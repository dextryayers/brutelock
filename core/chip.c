#include "chip.h"
#include "adb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char* lookup_soc(const char *raw, char *buf, size_t sz) {
    if (!raw || !raw[0]) { snprintf(buf, sz, "Unknown"); return buf; }
    struct { const char *pfx; const char *name; } db[] = {
        /* ===== Qualcomm Snapdragon 8-series ===== */
        {"sm8750", "Qualcomm Snapdragon 8 Elite (SM8750) 3nm"},
        {"sm8650", "Qualcomm Snapdragon 8 Gen 3 (SM8650) 4nm"},
        {"sm8650ab", "Qualcomm Snapdragon 8 Gen 3 for Galaxy (SM8650-AB) 4nm"},
        {"sm8635", "Qualcomm Snapdragon 8s Gen 3 (SM8635) 4nm"},
        {"sm8550", "Qualcomm Snapdragon 8 Gen 2 (SM8550) 4nm"},
        {"sm8550ab", "Qualcomm Snapdragon 8 Gen 2 for Galaxy (SM8550-AB) 4nm"},
        {"sm8575", "Qualcomm Snapdragon 8+ Gen 2 (SM8575) 4nm"},
        {"sm8475", "Qualcomm Snapdragon 8+ Gen 1 (SM8475) 4nm"},
        {"sm8450", "Qualcomm Snapdragon 8 Gen 1 (SM8450) 4nm"},
        {"sm8350", "Qualcomm Snapdragon 888+ (SM8350) 5nm"},
        {"sm8350ac", "Qualcomm Snapdragon 888+ (SM8350-AC) 5nm"},
        {"sm8325", "Qualcomm Snapdragon 888 4G (SM8325) 5nm"},
        {"sm8250", "Qualcomm Snapdragon 865+ (SM8250) 7nm"},
        {"sm8250ab", "Qualcomm Snapdragon 865+ (SM8250-AB) 7nm"},
        {"sm8150", "Qualcomm Snapdragon 855+ (SM8150) 7nm"},
        {"sm8150ac", "Qualcomm Snapdragon 855+ (SM8150-AC) 7nm"},
        {"sm8050", "Qualcomm Snapdragon 855 (SM8050) 7nm"},
        {"sm8250ac", "Qualcomm Snapdragon 870 (SM8250-AC) 7nm"},
        {"sdm865", "Qualcomm Snapdragon 865 (SDM865) 7nm"},
        {"sdm855", "Qualcomm Snapdragon 855 (SDM855) 7nm"},
        {"sdm850", "Qualcomm Snapdragon 850 (SDM850) 10nm"},
        {"sdm845", "Qualcomm Snapdragon 845 (SDM845) 10nm"},
        {"sdm840", "Qualcomm Snapdragon 840 (SDM840) 10nm"},
        {"sdm835", "Qualcomm Snapdragon 835 (MSM8998) 10nm"},
        {"sdm830", "Qualcomm Snapdragon 830 (MSM8998) 10nm"},
        {"sdm821", "Qualcomm Snapdragon 821 (MSM8996 Pro) 14nm"},
        {"sdm820", "Qualcomm Snapdragon 820 (MSM8996) 14nm"},
        {"sdm810", "Qualcomm Snapdragon 810 (MSM8994) 20nm"},
        {"sdm808", "Qualcomm Snapdragon 808 (MSM8992) 20nm"},
        {"sdm805", "Qualcomm Snapdragon 805 (APQ8084) 28nm"},
        {"sdm801", "Qualcomm Snapdragon 801 (MSM8974AC) 28nm"},
        {"sdm800", "Qualcomm Snapdragon 800 (MSM8974) 28nm"},

        /* ===== Qualcomm Snapdragon 7-series ===== */
        {"sm7675", "Qualcomm Snapdragon 7+ Gen 3 (SM7675) 4nm"},
        {"sm7550", "Qualcomm Snapdragon 7 Gen 3 (SM7550) 4nm"},
        {"sm7475", "Qualcomm Snapdragon 7+ Gen 2 (SM7475) 4nm"},
        {"sm7450", "Qualcomm Snapdragon 7 Gen 1 (SM7450) 4nm"},
        {"sm7375", "Qualcomm Snapdragon 7 Gen 1 (SM7375) 4nm"},
        {"sm7350", "Qualcomm Snapdragon 7s Gen 2 (SM7350) 4nm"},
        {"sm7325", "Qualcomm Snapdragon 778G+ (SM7325) 6nm"},
        {"sm7325ae", "Qualcomm Snapdragon 778G+ (SM7325-AE) 6nm"},
        {"sm7315", "Qualcomm Snapdragon 782G (SM7315) 6nm"},
        {"sm7315ab", "Qualcomm Snapdragon 782G (SM7315-AB) 6nm"},
        {"sm7250", "Qualcomm Snapdragon 765G (SM7250) 7nm"},
        {"sm7250ab", "Qualcomm Snapdragon 765G (SM7250-AB) 7nm"},
        {"sm7235", "Qualcomm Snapdragon 768G (SM7235) 7nm"},
        {"sm7150", "Qualcomm Snapdragon 730G (SM7150) 8nm"},
        {"sm7150ab", "Qualcomm Snapdragon 730G (SM7150-AB) 8nm"},
        {"sm7125", "Qualcomm Snapdragon 720G (SM7125) 8nm"},
        {"sm7118", "Qualcomm Snapdragon 732G (SM7118) 8nm"},
        {"sm7158", "Qualcomm Snapdragon 732G (SM7158) 8nm"},
        {"sm7050", "Qualcomm Snapdragon 6s Gen 3 (SM7050) 6nm"},
        {"sdm710", "Qualcomm Snapdragon 710 (SDM710) 10nm"},
        {"sdm712", "Qualcomm Snapdragon 712 (SDM712) 10nm"},
        {"sdm730", "Qualcomm Snapdragon 730 (SDM730) 8nm"},
        {"sdm750", "Qualcomm Snapdragon 750G (SM7225) 8nm"},
        {"sdm766", "Qualcomm Snapdragon 765 (SDM765) 7nm"},
        {"sdm768", "Qualcomm Snapdragon 768G 7nm"},
        {"sdm670", "Qualcomm Snapdragon 670 (SDM670) 10nm"},
        {"sdm675", "Qualcomm Snapdragon 675 (SDM675) 11nm"},
        {"sdm660", "Qualcomm Snapdragon 660 (SDM660) 14nm"},
        {"sdm650", "Qualcomm Snapdragon 650 (MSM8956) 28nm"},
        {"sdm652", "Qualcomm Snapdragon 652 (MSM8976) 28nm"},
        {"sdm653", "Qualcomm Snapdragon 653 (MSM8976 Pro) 28nm"},
        {"sdm636", "Qualcomm Snapdragon 636 (SDM636) 14nm"},
        {"sdm632", "Qualcomm Snapdragon 632 (SDM632) 14nm"},
        {"sdm630", "Qualcomm Snapdragon 630 (SDM630) 14nm"},
        {"sdm626", "Qualcomm Snapdragon 626 (MSM8953 Pro) 14nm"},
        {"sdm625", "Qualcomm Snapdragon 625 (MSM8953) 14nm"},
        {"sdm617", "Qualcomm Snapdragon 617 (MSM8952) 28nm"},
        {"sdm616", "Qualcomm Snapdragon 616 (MSM8939v2) 28nm"},
        {"sdm615", "Qualcomm Snapdragon 615 (MSM8939) 28nm"},
        {"sdm612", "Qualcomm Snapdragon 612 (MSM8938) 28nm"},
        {"sdm610", "Qualcomm Snapdragon 610 (MSM8936) 28nm"},

        /* ===== Qualcomm Snapdragon 4-series ===== */
        {"sm4450", "Qualcomm Snapdragon 4 Gen 2 (SM4450) 4nm"},
        {"sm4375", "Qualcomm Snapdragon 4 Gen 1 (SM4375) 6nm"},
        {"sm4350", "Qualcomm Snapdragon 480+ (SM4350) 8nm"},
        {"sm4350ac", "Qualcomm Snapdragon 480+ (SM4350-AC) 8nm"},
        {"sm4250", "Qualcomm Snapdragon 460 (SM4250) 11nm"},
        {"sdm439", "Qualcomm Snapdragon 439 (SDM439) 12nm"},
        {"sdm438", "Qualcomm Snapdragon 438 (SDM438) 12nm"},
        {"sdm450", "Qualcomm Snapdragon 450 (SDM450) 14nm"},
        {"sdm455", "Qualcomm Snapdragon 455 (SDM455) 14nm"},
        {"sdm429", "Qualcomm Snapdragon 429 (SDM429) 12nm"},
        {"sdm430", "Qualcomm Snapdragon 430 (MSM8937) 28nm"},
        {"sdm435", "Qualcomm Snapdragon 435 (MSM8940) 28nm"},
        {"sdm425", "Qualcomm Snapdragon 425 (MSM8917) 28nm"},
        {"sdm415", "Qualcomm Snapdragon 415 (MSM8929) 28nm"},
        {"sdm412", "Qualcomm Snapdragon 412 (MSM8916v2) 28nm"},
        {"sdm410", "Qualcomm Snapdragon 410 (MSM8916) 28nm"},
        {"sdm400", "Qualcomm Snapdragon 400 (MSM8226) 28nm"},
        {"sdm200", "Qualcomm Snapdragon 200 (MSM8610) 28nm"},
        {"sdm210", "Qualcomm Snapdragon 210 (MSM8909) 28nm"},
        {"sdm212", "Qualcomm Snapdragon 212 (MSM8909v2) 28nm"},

        /* ===== Qualcomm MSM legacy ===== */
        {"msmnile", "Qualcomm Snapdragon 865 (MSMNILE) 7nm"},
        {"msmsteppe", "Qualcomm Snapdragon 765G (MSMSTEPPE) 7nm"},
        {"msm8998", "Qualcomm Snapdragon 835 (MSM8998) 10nm"},
        {"msm8996", "Qualcomm Snapdragon 820/821 (MSM8996) 14nm"},
        {"msm8994", "Qualcomm Snapdragon 810 (MSM8994) 20nm"},
        {"msm8992", "Qualcomm Snapdragon 808 (MSM8992) 20nm"},
        {"msm8976", "Qualcomm Snapdragon 652/653 (MSM8976) 28nm"},
        {"msm8974", "Qualcomm Snapdragon 800/801 (MSM8974) 28nm"},
        {"msm8960", "Qualcomm Snapdragon S4 Plus (MSM8960) 28nm"},
        {"msm8953", "Qualcomm Snapdragon 625/626 (MSM8953) 14nm"},
        {"msm8952", "Qualcomm Snapdragon 617 (MSM8952) 28nm"},
        {"msm8956", "Qualcomm Snapdragon 650 (MSM8956) 28nm"},
        {"msm8940", "Qualcomm Snapdragon 435 (MSM8940) 28nm"},
        {"msm8939", "Qualcomm Snapdragon 615/616 (MSM8939) 28nm"},
        {"msm8937", "Qualcomm Snapdragon 430 (MSM8937) 28nm"},
        {"msm8929", "Qualcomm Snapdragon 415 (MSM8929) 28nm"},
        {"msm8917", "Qualcomm Snapdragon 425 (MSM8917) 28nm"},
        {"msm8916", "Qualcomm Snapdragon 410 (MSM8916) 28nm"},
        {"msm8909", "Qualcomm Snapdragon 210/212 (MSM8909) 28nm"},
        {"msm8226", "Qualcomm Snapdragon 400 (MSM8226) 28nm"},
        {"msm8610", "Qualcomm Snapdragon 200 (MSM8610) 28nm"},
        {"msm7630", "Qualcomm Snapdragon S3 (MSM7630) 45nm"},
        {"msm8260", "Qualcomm Snapdragon S3 (MSM8260) 45nm"},
        {"msm8660", "Qualcomm Snapdragon S3 (MSM8660) 45nm"},
        {"msm8255", "Qualcomm Snapdragon S2 (MSM8255) 45nm"},
        {"msm8655", "Qualcomm Snapdragon S2 (MSM8655) 45nm"},
        {"msm7225", "Qualcomm Snapdragon S1 (MSM7225) 65nm"},
        {"msm7227", "Qualcomm Snapdragon S1 (MSM7227) 65nm"},
        {"msm7627", "Qualcomm Snapdragon S1 (MSM7627) 65nm"},

        /* ===== Qualcomm APQ series ===== */
        {"apq8098", "Qualcomm Snapdragon 845 (APQ8098) 10nm"},
        {"apq8096", "Qualcomm Snapdragon 820/821 (APQ8096) 14nm"},
        {"apq8074", "Qualcomm Snapdragon 800 (APQ8074) 28nm"},
        {"apq8064", "Qualcomm Snapdragon S4 Pro (APQ8064) 28nm"},

        /* ===== Qualcomm QCS/IPQ ===== */
        {"qcs8250", "Qualcomm QCS8250 (Snapdragon 865) 7nm"},
        {"qcs610", "Qualcomm QCS610 8nm"},
        {"qcs605", "Qualcomm QCS605 10nm"},
        {"qcs603", "Qualcomm QCS603 10nm"},
        {"ipq8074", "Qualcomm IPQ8074 WiFi 6 SoC 12nm"},
        {"ipq6018", "Qualcomm IPQ6018 14nm"},
        {"ipq5018", "Qualcomm IPQ5018 14nm"},

        /* ===== MediaTek Dimensity ===== */
        {"mt6989", "MediaTek Dimensity 9400 (MT6989) 3nm"},
        {"mt6985", "MediaTek Dimensity 9300 (MT6985) 4nm"},
        {"mt6983", "MediaTek Dimensity 9200+ (MT6983) 4nm"},
        {"mt6980", "MediaTek Dimensity 9000 (MT6980) 4nm"},
        {"mt6899", "MediaTek Dimensity 9300+ (MT6899) 4nm"},
        {"mt6897", "MediaTek Dimensity 8400 (MT6897) 4nm"},
        {"mt6896", "MediaTek Dimensity 8400 (MT6896) 4nm"},
        {"mt6895", "MediaTek Dimensity 8300 (MT6895) 4nm"},
        {"mt6893", "MediaTek Dimensity 8200 (MT6893) 4nm"},
        {"mt6891", "MediaTek Dimensity 8100 (MT6891) 5nm"},
        {"mt6890", "MediaTek Dimensity 8000 (MT6890) 5nm"},
        {"mt6889", "MediaTek Dimensity 1300 (MT6889) 6nm"},
        {"mt6886", "MediaTek Dimensity 1200 (MT6886) 6nm"},
        {"mt6883", "MediaTek Dimensity 1100 (MT6883) 6nm"},
        {"mt6880", "MediaTek Dimensity 1000+ (MT6880) 7nm"},
        {"mt6879", "MediaTek Dimensity 7050 (MT6879) 6nm"},
        {"mt6878", "MediaTek Dimensity 7200 (MT6878) 4nm"},
        {"mt6877", "MediaTek Dimensity 900 (MT6877) 6nm"},
        {"mt6875", "MediaTek Dimensity 920 (MT6875) 6nm"},
        {"mt6873", "MediaTek Dimensity 800 (MT6873) 7nm"},
        {"mt6871", "MediaTek Dimensity 820 (MT6871) 7nm"},
        {"mt6855", "MediaTek Dimensity 6300 (MT6855) 6nm"},
        {"mt6853", "MediaTek Dimensity 720 (MT6853) 7nm"},
        {"mt6853t", "MediaTek Dimensity 720 (MT6853T) 7nm"},
        {"mt685v", "MediaTek Dimensity 6080 (MT6835) 6nm"},
        {"mt6835", "MediaTek Dimensity 6100+ (MT6835) 6nm"},
        {"mt6833", "MediaTek Dimensity 700 (MT6833) 7nm"},
        {"mt6833p", "MediaTek Dimensity 700P (MT6833P) 7nm"},
        {"mt6825", "MediaTek Dimensity 6020 (MT6825) 7nm"},
        {"mt6813", "MediaTek Dimensity 5G Entry Level (MT6813) 6nm"},
        {"mt6789", "MediaTek Helio G99 (MT6789) 6nm"},
        {"mt6788", "MediaTek Helio G91 (MT6788) 12nm"},
        {"mt6785", "MediaTek Helio G90/G95 (MT6785) 12nm"},
        {"mt6781", "MediaTek Helio G96 (MT6781) 12nm"},
        {"mt6780", "MediaTek Helio G88 (MT6780) 12nm"},
        {"mt6779", "MediaTek Helio P90 (MT6779) 12nm"},
        {"mt6778", "MediaTek Helio P65 (MT6778) 12nm"},
        {"mt6775", "MediaTek Helio P95 (MT6775) 12nm"},
        {"mt6771", "MediaTek Helio P60/P70 (MT6771) 12nm"},
        {"mt6769", "MediaTek Helio G70/G80 (MT6769) 12nm"},
        {"mt6768", "MediaTek Helio G85 (MT6768) 12nm"},
        {"mt6765", "MediaTek Helio P35 (MT6765) 12nm"},
        {"mt6763", "MediaTek Helio P23 (MT6763) 16nm"},
        {"mt6761", "MediaTek Helio A22 (MT6761) 12nm"},
        {"mt6762", "MediaTek Helio P22 (MT6762) 12nm"},
        {"mt6759", "MediaTek Helio P25 (MT6759) 16nm"},
        {"mt6757", "MediaTek Helio P20 (MT6757) 16nm"},
        {"mt6756", "MediaTek Helio P15 (MT6756) 28nm"},
        {"mt6755", "MediaTek Helio P10 (MT6755) 28nm"},
        {"mt6753", "MediaTek MT6753 28nm"},
        {"mt6752", "MediaTek MT6752 28nm"},
        {"mt6750", "MediaTek MT6750 28nm"},
        {"mt6739", "MediaTek Helio A22 (MT6739) 28nm"},
        {"mt6738", "MediaTek MT6738 28nm"},
        {"mt6737", "MediaTek MT6737 28nm"},
        {"mt6735", "MediaTek MT6735 28nm"},
        {"mt6732", "MediaTek MT6732 28nm"},

        /* ===== MediaTek MT65xx legacy ===== */
        {"mt6595", "MediaTek MT6595 28nm"},
        {"mt6592", "MediaTek MT6592 (8-core A7) 28nm"},
        {"mt6589", "MediaTek MT6589 28nm"},
        {"mt6582", "MediaTek MT6582 28nm"},
        {"mt6580", "MediaTek MT6580 28nm"},
        {"mt6572", "MediaTek MT6572 28nm"},
        {"mt6575", "MediaTek MT6575 40nm"},
        {"mt6577", "MediaTek MT6577 40nm"},

        /* ===== MediaTek Kompanio ===== */
        {"mt8196", "MediaTek Kompanio 520 (MT8196) 6nm"},
        {"mt8195", "MediaTek Kompanio 1200 (MT8195) 6nm"},
        {"mt8192", "MediaTek Kompanio 820 (MT8192) 7nm"},
        {"mt8188", "MediaTek Kompanio 838 (MT8188) 7nm"},
        {"mt8186", "MediaTek Kompanio 828 (MT8186) 12nm"},
        {"mt8183", "MediaTek Kompanio 500 (MT8183) 12nm"},
        {"mt8176", "MediaTek Kompanio 300 (MT8176) 28nm"},
        {"mt8173", "MediaTek MT8173 28nm"},

        /* ===== MediaTek IoT ===== */
        {"mt8516", "MediaTek MT8516 28nm"},
        {"mt8512", "MediaTek MT8512 28nm"},
        {"mt8167", "MediaTek MT8167 28nm"},
        {"mt8163", "MediaTek MT8163 28nm"},
        {"mt8127", "MediaTek MT8127 28nm"},

        /* ===== Samsung Exynos ===== */
        {"s5e9945", "Samsung Exynos 2400 (S5E9945) 4nm"},
        {"s5e9935", "Samsung Exynos 2400 (S5E9935) 4nm"},
        {"s5e9925", "Samsung Exynos 2200 (S5E9925) 4nm"},
        {"s5e9922", "Samsung Exynos 2200 (S5E9922) 4nm"},
        {"s5e9845", "Samsung Exynos 2100 (S5E9845) 5nm"},
        {"s5e9835", "Samsung Exynos 2100 (S5E9835) 5nm"},
        {"s5e9825", "Samsung Exynos 9825 (S5E9825) 7nm"},
        {"s5e9820", "Samsung Exynos 9820 (S5E9820) 8nm"},
        {"s5e9815", "Samsung Exynos 9810 (S5E9815) 10nm"},
        {"s5e9811", "Samsung Exynos 9811 (S5E9811) 10nm"},
        {"s5e9610", "Samsung Exynos 9610/9611 (S5E9610) 10nm"},
        {"s5e9630", "Samsung Exynos 9630 5nm"},
        {"s5e9930", "Samsung Exynos 2200 (S5E9930) 4nm"},
        {"s5e8845", "Samsung Exynos 8895 (S5E8845) 10nm"},
        {"s5e8895", "Samsung Exynos 8895 (S5E8895) 10nm"},
        {"exynos8890", "Samsung Exynos 8890 14nm"},
        {"s5e8890", "Samsung Exynos 8890 14nm"},
        {"exynos7885", "Samsung Exynos 7885 14nm"},
        {"exynos7884", "Samsung Exynos 7884/7884B 14nm"},
        {"exynos7880", "Samsung Exynos 7880 14nm"},
        {"exynos7872", "Samsung Exynos 7872 14nm"},
        {"exynos7870", "Samsung Exynos 7870 14nm"},
        {"exynos7860", "Samsung Exynos 7860 14nm"},
        {"exynos7650", "Samsung Exynos 7650 12nm"},
        {"exynos7630", "Samsung Exynos 7630 12nm"},
        {"exynos7580", "Samsung Exynos 7580 28nm"},
        {"exynos7420", "Samsung Exynos 7420 14nm"},
        {"exynos7430", "Samsung Exynos 7430 14nm"},
        {"exynos7270", "Samsung Exynos 7270 14nm"},
        {"exynos7260", "Samsung Exynos 7260 14nm"},
        {"exynos7250", "Samsung Exynos 7250 14nm"},
        {"exynos5260", "Samsung Exynos 5260 (Hexa) 28nm"},
        {"exynos5250", "Samsung Exynos 5250 (Dual A15) 32nm"},
        {"exynos5422", "Samsung Exynos 5422 (Octa) 28nm"},
        {"exynos5420", "Samsung Exynos 5420 (Octa) 28nm"},
        {"exynos5410", "Samsung Exynos 5410 (Octa) 28nm"},
        {"exynos5800", "Samsung Exynos 5800 28nm"},
        {"exynos4212", "Samsung Exynos 4212 32nm"},
        {"exynos4210", "Samsung Exynos 4210 (Orion) 45nm"},
        {"exynos4412", "Samsung Exynos 4412 32nm"},
        {"exynos3110", "Samsung Exynos 3110 (Hummingbird) 45nm"},

        /* ===== HiSilicon Kirin ===== */
        {"kirin9010", "HiSilicon Kirin 9010 7nm"},
        {"kirin9000s", "HiSilicon Kirin 9000S 7nm"},
        {"kirin9000", "HiSilicon Kirin 9000 5nm"},
        {"kirin990", "HiSilicon Kirin 990 5G 7nm"},
        {"kirin990e", "HiSilicon Kirin 990E 7nm"},
        {"kirin985", "HiSilicon Kirin 985 5G 7nm"},
        {"kirin980", "HiSilicon Kirin 980 7nm"},
        {"kirin975", "HiSilicon Kirin 975 7nm"},
        {"kirin970", "HiSilicon Kirin 970 10nm"},
        {"kirin960", "HiSilicon Kirin 960 16nm"},
        {"kirin955", "HiSilicon Kirin 955 16nm"},
        {"kirin950", "HiSilicon Kirin 950 16nm"},
        {"kirin935", "HiSilicon Kirin 935 28nm"},
        {"kirin930", "HiSilicon Kirin 930 28nm"},
        {"kirin925", "HiSilicon Kirin 925 28nm"},
        {"kirin920", "HiSilicon Kirin 920 28nm"},
        {"kirin910t", "HiSilicon Kirin 910T 28nm"},
        {"kirin910", "HiSilicon Kirin 910 28nm"},
        {"kirin659", "HiSilicon Kirin 659 16nm"},
        {"kirin658", "HiSilicon Kirin 658 16nm"},
        {"kirin655", "HiSilicon Kirin 655 16nm"},
        {"kirin650", "HiSilicon Kirin 650 16nm"},

        /* ===== Google Tensor ===== */
        {"tensor5", "Google Tensor G5 3nm"},
        {"tensor4", "Google Tensor G4 4nm"},
        {"tensor3", "Google Tensor G3 4nm"},
        {"tensor2", "Google Tensor G2 5nm"},
        {"tensor", "Google Tensor G1 5nm"},
        {"gs201", "Google Tensor G2 5nm"},
        {"gs101", "Google Tensor G1 5nm"},
        {"gs501", "Google Tensor G4 4nm"},

        /* ===== Apple A-series ===== */
        {"t8140", "Apple A18 Pro 3nm"},
        {"t8130", "Apple A18 3nm"},
        {"t8120", "Apple A17 Pro 3nm"},
        {"t8116", "Apple A16 Bionic 4nm"},
        {"t8103", "Apple A17 Pro 3nm"},
        {"t8110", "Apple A15 Bionic 5nm"},
        {"t8101", "Apple A14 Bionic 5nm"},
        {"t8100", "Apple A14 Bionic 5nm"},
        {"t8030", "Apple A13 Bionic 7nm"},
        {"t8020", "Apple A12X Bionic 7nm"},
        {"t8015", "Apple A12 Bionic 7nm"},
        {"t8012", "Apple A11 Bionic 10nm"},
        {"t8011", "Apple A10X Fusion 10nm"},
        {"t8010", "Apple A10 Fusion 16nm"},
        {"s8000", "Apple A9 14nm"},
        {"s8003", "Apple A9X 16nm"},
        {"s5l8960", "Apple A7 28nm"},
        {"s5l8950", "Apple A6X 32nm"},
        {"s5l8955", "Apple A6 32nm"},

        /* ===== Apple M-series ===== */
        {"tm8119", "Apple M4 Pro 3nm"},
        {"tm8117", "Apple M4 3nm"},
        {"tm8110", "Apple M3 Max 3nm"},
        {"tm8109", "Apple M3 Pro 3nm"},
        {"tm8108", "Apple M3 3nm"},
        {"tm8118", "Apple M3 3nm"},
        {"tm8105", "Apple M2 Ultra 5nm"},
        {"tm8104", "Apple M2 Max 5nm"},
        {"tm8103", "Apple M2 Pro 5nm"},
        {"tm8102", "Apple M2 5nm"},
        {"tm8101", "Apple M1 Ultra 5nm"},
        {"tm8100", "Apple M1 Max 5nm"},
        {"t8103", "Apple M1 Pro 5nm"},
        {"t8102", "Apple M1 5nm"},

        /* ===== Unisoc / Spreadtrum ===== */
        {"ums9630", "Unisoc T770 (Tangula 770) 6nm"},
        {"ums9620", "Unisoc T760 (Tangula 760) 6nm"},
        {"ums9230", "Unisoc T820 6nm"},
        {"ums9220", "Unisoc T720 6nm"},
        {"ums9120", "Unisoc T618 12nm"},
        {"ums9110", "Unisoc T610 12nm"},
        {"ums9100", "Unisoc T606 12nm"},
        {"ums9230e", "Unisoc T830 6nm"},
        {"ums512", "Unisoc SC9863A (4G) 28nm"},
        {"ums312", "Unisoc SC9832E 28nm"},
        {"ums311", "Unisoc SC9820E 28nm"},
        {"sc9863a", "Unisoc SC9863A 28nm"},
        {"sc9850k", "Unisoc SC9850K 28nm"},
        {"sc9840", "Unisoc SC9840 28nm"},
        {"sc9832e", "Unisoc SC9832E 28nm"},
        {"sc9820e", "Unisoc SC9820E 28nm"},
        {"sc7731e", "Spreadtrum SC7731E 28nm"},
        {"sc7721", "Spreadtrum SC7721 28nm"},
        {"sc7715", "Spreadtrum SC7715 28nm"},
        {"sc6820", "Spreadtrum SC6820 40nm"},
        {"spreadtrum", "Spreadtrum/Unisoc SoC (generic)"},
        {"t760", "Unisoc T760 6nm"},
        {"t770", "Unisoc T770 6nm"},
        {"t820", "Unisoc T820 6nm"},
        {"t618", "Unisoc T618 12nm"},
        {"t610", "Unisoc T610 12nm"},
        {"t606", "Unisoc T606 12nm"},

        /* ===== Rockchip ===== */
        {"rk3588", "Rockchip RK3588 8nm"},
        {"rk3588s", "Rockchip RK3588S 8nm"},
        {"rk3568", "Rockchip RK3568 22nm"},
        {"rk3566", "Rockchip RK3566 22nm"},
        {"rk3399", "Rockchip RK3399 (Dual A72+Quad A53) 28nm"},
        {"rk3328", "Rockchip RK3328 28nm"},
        {"rk3308", "Rockchip RK3308 28nm"},
        {"rk3288", "Rockchip RK3288 28nm"},
        {"rk3229", "Rockchip RK3229 28nm"},
        {"rk3228", "Rockchip RK3228 28nm"},
        {"rk3188", "Rockchip RK3188 28nm"},
        {"rk3066", "Rockchip RK3066 40nm"},
        {"rk2928", "Rockchip RK2928 40nm"},
        {"rk2918", "Rockchip RK2918 40nm"},
        {"rk2808", "Rockchip RK2808 65nm"},
        {"rk1808", "Rockchip RK1808 (NPU) 28nm"},
        {"rv1126", "Rockchip RV1126 14nm"},
        {"rv1109", "Rockchip RV1109 14nm"},
        {"rv1106", "Rockchip RV1106 14nm"},

        /* ===== Allwinner ===== */
        {"sun55iw3", "Allwinner A523 22nm"},
        {"sun50iw9", "Allwinner H616 28nm"},
        {"sun50iw6", "Allwinner H6 28nm"},
        {"sun50iw3", "Allwinner H5 28nm"},
        {"sun50iw2", "Allwinner A64 40nm"},
        {"sun50iw1", "Allwinner A53-based (A64) 40nm"},
        {"sun8iw15", "Allwinner H313 28nm"},
        {"sun8iw12", "Allwinner H3 28nm"},
        {"sun8iw7", "Allwinner H2+ 28nm"},
        {"sun8iw6", "Allwinner H3 28nm"},
        {"sun8iw5", "Allwinner A33 28nm"},
        {"sun8iw3", "Allwinner A23 28nm"},
        {"sun8iw1", "Allwinner A31s 40nm"},
        {"sun7iw1", "Allwinner A20 40nm"},
        {"sun6iw1", "Allwinner A31 40nm"},
        {"sun5iw1", "Allwinner A13 40nm"},
        {"sun4iw1", "Allwinner A10 55nm"},
        {"allwinner", "Allwinner SoC (generic)"},

        /* ===== Amlogic ===== */
        {"s928x", "Amlogic S928X 12nm"},
        {"s922x", "Amlogic S922X (Amlogic A311D) 12nm"},
        {"s912", "Amlogic S912 28nm"},
        {"s905y4", "Amlogic S905Y4 12nm"},
        {"s905x4", "Amlogic S905X4 12nm"},
        {"s905x3", "Amlogic S905X3 12nm"},
        {"s905x2", "Amlogic S905X2 12nm"},
        {"s905x", "Amlogic S905X 28nm"},
        {"s905d", "Amlogic S905D 28nm"},
        {"s905", "Amlogic S905 28nm"},
        {"s905w", "Amlogic S905W 28nm"},
        {"s912", "Amlogic S912 28nm"},
        {"s802", "Amlogic S802 28nm"},
        {"s805", "Amlogic S805 28nm"},
        {"s812", "Amlogic S812 28nm"},

        /* ===== NVIDIA Tegra ===== */
        {"tegra194", "NVIDIA Tegra Xavier (T194) 12nm"},
        {"tegra186", "NVIDIA Tegra X2 (T186) 16nm"},
        {"tegra210", "NVIDIA Tegra X1 (T210) 20nm"},
        {"tegra132", "NVIDIA Tegra K1 (Denver) 28nm"},
        {"tegra124", "NVIDIA Tegra K1 (T124) 28nm"},
        {"tegra114", "NVIDIA Tegra 4 (T114) 28nm"},
        {"tegra4", "NVIDIA Tegra 4 (T114) 28nm"},
        {"tegra3", "NVIDIA Tegra 3 (T30) 40nm"},
        {"tegra2", "NVIDIA Tegra 2 (T20) 40nm"},
        {"tegra250", "NVIDIA Tegra 250 40nm"},

        /* ===== Intel Atom / SoFIA ===== */
        {"sofia3gr", "Intel Atom x3-C3200 (SoFIA 3G) 28nm"},
        {"sofia3g", "Intel Atom x3-C3130 (SoFIA 3G) 28nm"},
        {"sofia", "Intel Atom x3 (SoFIA) 28nm"},
        {"moorfield", "Intel Atom Z3460 (Moorfield) 28nm"},
        {"tangier", "Intel Atom Z3560 (Tangier) 28nm"},
        {"cloverview", "Intel Atom Z2560 (Clover Trail+) 32nm"},
        {"salitown", "Intel Atom Z2420 (Salitown) 32nm"},
        {"medfield", "Intel Atom Z2460 (Medfield) 32nm"},
        {"baytrail", "Intel Atom Z3735 (Bay Trail-T) 22nm"},
        {"cherrytrail", "Intel Atom x5-Z8300 (Cherry Trail) 14nm"},
        {"broxton", "Intel Atom x7-Z8750 (Broxton) 14nm"},

        /* ===== Samsung Exynos Auto / Modem ===== */
        {"exynos8", "Samsung Exynos 8-series Auto"},
        {"exynos7", "Samsung Exynos 7-series"},
        {"exynos5", "Samsung Exynos 5-series"},
        {"exynos4", "Samsung Exynos 4-series"},
        {"exynos3", "Samsung Exynos 3-series"},

        /* ===== Samsung BCM ===== */
        {"bcm23550", "Broadcom BCM23550 28nm"},
        {"bcm21664", "Broadcom BCM21664 40nm"},
        {"bcm28155", "Broadcom BCM28155 40nm"},

        /* ===== Leadcore ===== */
        {"lc1860", "Leadcore LC1860 28nm"},
        {"lc1913", "Leadcore LC1913 28nm"},

        /* ===== Ingenic ===== */
        {"x2000", "Ingenic X2000 28nm"},
        {"x1000", "Ingenic X1000 28nm"},
        {"jz4780", "Ingenic JZ4780 28nm"},
        {"jz4775", "Ingenic JZ4775 28nm"},
        {"jz4760", "Ingenic JZ4760 28nm"},

        /* ===== Marvell / NXP / TI ===== */
        {"pxa1908", "Marvell PXA1908 28nm"},
        {"pxa1928", "Marvell PXA1928 28nm"},
        {"pxa1088", "Marvell PXA1088 40nm"},
        {"imx8ulp", "NXP i.MX8ULP 28nm"},
        {"imx8mm", "NXP i.MX8M Mini 14nm"},
        {"imx8mq", "NXP i.MX8M Quad 14nm"},
        {"imx6q", "NXP i.MX6 Quad 40nm"},
        {"omap4470", "TI OMAP4470 45nm"},
        {"omap4460", "TI OMAP4460 45nm"},
        {"omap4430", "TI OMAP4430 45nm"},
        {"omap3630", "TI OMAP3630 45nm"},

        /* ===== Qualcomm QSD legacy ===== */
        {"qsd8250", "Qualcomm Snapdragon QSD8250 (1GHz) 65nm"},
        {"qsd8650", "Qualcomm Snapdragon QSD8650 65nm"},

        /* ===== Qualcomm IPQ networking ===== */
        {"ipq8074", "Qualcomm IPQ8074 (Quad A53) 14nm"},

        /* ===== MediaTek MT76xx WiFi ===== */
        {"mt7622", "MediaTek MT7622 (Dual A53) 28nm"},
        {"mt7621", "MediaTek MT7621 (Dual MIPS) 28nm"},
        {"mt7620", "MediaTek MT7620 (MIPS) 28nm"},
        {"mt7688", "MediaTek MT7688 (MIPS) 28nm"},

        /* ===== Broadcom mobiles ===== */
        {"bcm2712", "Broadcom BCM2712 (RPi 5) 16nm"},
        {"bcm2711", "Broadcom BCM2711 (RPi 4) 28nm"},
        {"bcm2837", "Broadcom BCM2837 (RPi 3) 40nm"},
        {"bcm2836", "Broadcom BCM2836 (RPi 2) 40nm"},
        {"bcm2835", "Broadcom BCM2835 (RPi 1/Zero) 40nm"},
        {"bcm2355", "Broadcom BCM23550 28nm"},

        /* ===== Xiaomi Surge ===== */
        {"surge s1", "Xiaomi Surge S1 28nm"},
        {"surge s2", "Xiaomi Surge S2 28nm"},
        {"surge c1", "Xiaomi Surge C1 (ISP) 14nm"},
        {"surge g1", "Xiaomi Surge G1 14nm"},
        {"surge p1", "Xiaomi Surge P1 (charger) 22nm"},

        /* ===== End ===== */
        {NULL,NULL}
    };

    char low[128];
    int rl = 0;
    while (raw[rl] && rl < 127) { low[rl] = tolower((unsigned char)raw[rl]); rl++; }
    low[rl] = 0;

    for (int i = 0; db[i].pfx; i++) {
        if (strstr(low, db[i].pfx)) {
            strncpy(buf, db[i].name, sz-1); buf[sz-1] = 0;
            return buf;
        }
    }
    snprintf(buf, sz, "%s (generic)", raw);
    return buf;
}

void detect_chip(DeviceInfo *info) {
    char *tmp;
    char raw[128] = {0};
    int found = 0;

    tmp = adb_shell("adb shell getprop ro.chipset.name 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) { strncpy(raw, tmp, sizeof(raw)-1); found = 1; }

    if (!found) {
        tmp = adb_shell("adb shell getprop ro.board.platform 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) { strncpy(raw, tmp, sizeof(raw)-1); found = 1; }
    }
    if (!found) {
        tmp = adb_shell("adb shell getprop ro.product.board 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) { strncpy(raw, tmp, sizeof(raw)-1); found = 1; }
    }
    if (!found) {
        tmp = adb_shell("adb shell getprop ro.mediatek.platform 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) { strncpy(raw, tmp, sizeof(raw)-1); found = 1; }
    }
    if (!found) {
        tmp = adb_shell("adb shell getprop ro.hardware 2>/dev/null");
        trim_newline(tmp);
        if (strlen(tmp) > 0) { strncpy(raw, tmp, sizeof(raw)-1); found = 1; }
    }
    lookup_soc(raw, info->soc_exact, sizeof(info->soc_exact));

    tmp = adb_shell("adb shell cat /proc/cpuinfo 2>/dev/null | grep -E 'Hardware|Processor|CPU implementer|CPU part' | head -10");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        int count = 0;
        char *p = tmp;
        while (*p) { if (*p == '\n') count++; p++; }
        snprintf(info->cpu_clusters, sizeof(info->cpu_clusters), "%s (%d lines)", tmp, count);
    }

    tmp = adb_shell("adb shell cat /sys/kernel/gpu/gpu_freq_table 2>/dev/null | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) {
        snprintf(info->gpu_freq, sizeof(info->gpu_freq), "%s MHz", tmp);
    }
    if (strlen(info->gpu_freq) == 0) {
        tmp = adb_shell("adb shell cat /proc/gpufreq/gpufreq_opp_dump 2>/dev/null | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->gpu_freq, tmp, sizeof(info->gpu_freq)-1);
    }

    tmp = adb_shell("adb shell dumpsys media.player 2>/dev/null | grep -i npu | head -1");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->npu, tmp, sizeof(info->npu)-1);
    if (strlen(info->npu) == 0) {
        tmp = adb_shell("adb shell dumpsys media 2>/dev/null | grep -i -E 'apu|npu|dsp' | head -3");
        trim_newline(tmp);
        if (strlen(tmp) > 0) strncpy(info->npu, tmp, sizeof(info->npu)-1);
    }

    tmp = adb_shell("adb shell getprop ro.vendor.modem.name 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) == 0)
        tmp = adb_shell("adb shell getprop gsm.version.baseband 2>/dev/null");
    trim_newline(tmp);
    if (strlen(tmp) > 0) strncpy(info->modem_type, tmp, sizeof(info->modem_type)-1);
}

void print_chip_info(const DeviceInfo *info) {
    printf("  " BOLD "%-20s" RESET ": %s\n", "SoC Model", info->soc_exact);
    if (strlen(info->cpu_clusters))
        printf("  " BOLD "%-20s" RESET ": %s\n", "CPU Info", info->cpu_clusters);
    if (strlen(info->gpu_freq))
        printf("  " BOLD "%-20s" RESET ": %s\n", "GPU Freq", info->gpu_freq);
    if (strlen(info->npu))
        printf("  " BOLD "%-20s" RESET ": %s\n", "NPU/APU/DSP", info->npu);
    if (strlen(info->modem_type))
        printf("  " BOLD "%-20s" RESET ": %s\n", "Modem", info->modem_type);
}
