#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "platform.h"
#include "main.h"
#include "mecha.h"
#include "eeprom.h"
#include "updates.h"

static int DumpEEPROM(const char *filename)
{
    FILE *dump;
    int i, progress, result;
    u16 data;

    PlatShowMessage("\nDumping EEPROM:\n");
    if ((dump = fopen(filename, "wb")) != NULL)
    {
        for (i = 0; i < 1024 / 2; i++)
        {
            putchar('\r');
            PlatShowMessage("Progress: ");
            putchar('[');
            for (progress = 0; progress <= (i * 20 / 512); progress++)
                putchar('#');
            for (; progress < (512 * 20 / 512); progress++)
                putchar(' ');
            putchar(']');

            if ((result = EEPROMReadWord(i, &data)) != 0)
            {
                PlatShowMessage("EEPROM read error %d:%d\n", i, result);
                break;
            }
            if (fwrite(&data, sizeof(u16), 1, dump) != 1)
                break;
        }
        putchar('\n');

        fclose(dump);
    }
    else
        result = -EIO;

    return result;
}

static int RestoreEEPROM(const char *filename)
{
    FILE *dump;
    int i, progress, result;
    u16 data;

    PlatShowMessage("\nRestoring EEPROM:\n");
    if ((dump = fopen(filename, "rb")) != NULL)
    {
        for (i = 0; i < 1024 / 2; i++)
        {
            putchar('\r');
            PlatShowMessage("Progress: ");
            putchar('[');
            for (progress = 0; progress <= (i * 20 / 512); progress++)
                putchar('#');
            for (; progress < (512 * 20 / 512); progress++)
                putchar(' ');
            putchar(']');

            if (fread(&data, sizeof(u16), 1, dump) != 1)
                break;

            if ((result = EEPROMWriteWord(i, data)) != 0)
            {
                PlatShowMessage("EEPROM write error %d:%d\n", i, result);
                break;
            }
        }
        putchar('\n');

        fclose(dump);
    }
    else
        result = -ENOENT;

    return result;
}

#define EEPROM_UPDATE_FLAG_SANYO    1 // Supports SANYO OP
#define EEPROM_UPDATE_FLAG_NEW_SONY 2 // No support for the old T487

static int UpdateEEPROM(int chassis)
{
    struct UpdateData
    {
        int (*update)(int ClearOSD2InitBit, int ReplacedMecha, int lens, int opt);
        unsigned int flags;
    };
    int ClearOSD2InitBit, ReplacedMecha, OpticalBlock, ObjectLens, result;
    char choice;
    struct UpdateData *selected;
    struct UpdateData data[MECHA_CHASSIS_MODEL_COUNT] = {
        {&MechaUpdateChassisCex10000, 0},
        {&MechaUpdateChassisA, 0},
        {&MechaUpdateChassisAB, 0},
        {&MechaUpdateChassisB, 0},
        {&MechaUpdateChassisC, 0},
        {&MechaUpdateChassisD, 0},
        {&MechaUpdateChassisF, EEPROM_UPDATE_FLAG_SANYO},
        {&MechaUpdateChassisG, EEPROM_UPDATE_FLAG_SANYO | EEPROM_UPDATE_FLAG_NEW_SONY},
        {&MechaUpdateChassisH, EEPROM_UPDATE_FLAG_SANYO | EEPROM_UPDATE_FLAG_NEW_SONY},
        {&MechaUpdateChassisDexA, 0},
        {&MechaUpdateChassisDexA2, 0},
        {&MechaUpdateChassisDexA3, 0},
        {&MechaUpdateChassisDexB, 0},
        {&MechaUpdateChassisDexD, 0},
        {&MechaUpdateChassisDexH, EEPROM_UPDATE_FLAG_SANYO | EEPROM_UPDATE_FLAG_NEW_SONY},
    };

    PlatShowMessage("Update EEPROM\n\n");
    if (chassis >= 0)
    {
        selected = &data[chassis];

        do
        {
            PlatShowMessage("Was the MECHACON replaced (y/n)? ");
            choice = getchar();
            while (getchar() != '\n')
            {
            };
        } while (choice != 'y' && choice != 'n');
        ReplacedMecha = choice == 'y';

        if (selected->flags & EEPROM_UPDATE_FLAG_SANYO)
        {
            do
            {
                PlatShowMessage("Please select the optical block:\n"
                                "\t1. SONY\n"
                                "\t2. SANYO\n"
                                "Your choice: ");
                OpticalBlock = 0;
                if (scanf("%d", &OpticalBlock))
                    while (getchar() != '\n')
                    {
                    };
            } while (OpticalBlock < 1 || OpticalBlock > 2);
            OpticalBlock--;
        }
        else
            OpticalBlock = MECHA_OP_SONY;

        if (!(selected->flags & EEPROM_UPDATE_FLAG_NEW_SONY) && (OpticalBlock != MECHA_OP_SANYO))
        {
            do
            {
                PlatShowMessage("Please select the object lens:\n"
                                "\t1. T487\n"
                                "\t2. T609K\n"
                                "Your choice: ");
                ObjectLens = 0;
                if (scanf("%d", &ObjectLens))
                    while (getchar() != '\n')
                    {
                    };
            } while (ObjectLens < 1 || ObjectLens > 2);
            ObjectLens--;
        }
        else
            ObjectLens = MECHA_LENS_T487;

        if (EEPROMCanClearOSD2InitBit(chassis))
        {
            do
            {
                PlatShowMessage("The OSD2 init bit is set. Clear it? (y/n)");
                choice = getchar();
                while (getchar() != '\n')
                {
                };
            } while (choice != 'y' && choice != 'n');
            ClearOSD2InitBit = choice == 'y';
        }
        else
            ClearOSD2InitBit = 0;

        if ((result = selected->update(ClearOSD2InitBit, ReplacedMecha, ObjectLens, OpticalBlock)) > 0)
        {
            PlatShowMessage("Actions available:\n");
            if (result & UPDATE_REGION_EEP_ECR)
                PlatShowMessage("\tEEPROM ECR\n");
            if (result & UPDATE_REGION_DISCDET)
                PlatShowMessage("\tDisc detect\n");
            if (result & UPDATE_REGION_SERVO)
                PlatShowMessage("\tServo\n");
            if (result & UPDATE_REGION_TILT)
                PlatShowMessage("\tAuto-tilt\n");
            if (result & UPDATE_REGION_TRAY)
                PlatShowMessage("\tTray\n");
            if (result & UPDATE_REGION_EEGS)
                PlatShowMessage("\tEE & GS\n");
            if (result & UPDATE_REGION_ECR)
                PlatShowMessage("\tRTC ECR\n");
            if (result & UPDATE_REGION_RTC)
            {
                PlatShowMessage("\tRTC:\n");
                if (result & UPDATE_REGION_RTC_CTL12)
                    PlatShowMessage("\t\tRTC CTL1,2 ERROR\n");
                if (result & UPDATE_REGION_RTC_TIME)
                    PlatShowMessage("\t\tRTC TIME ERROR\n");
            }
            if (result & UPDATE_REGION_DEFAULTS)
                PlatShowMessage("\tMechacon defaults\n");

            do
            {
                PlatShowMessage("Proceed with updates? (y/n) ");
                choice = getchar();
                while (getchar() != '\n')
                {
                };
            } while (choice != 'y' && choice != 'n');
            if (choice == 'y')
            {
                return MechaCommandExecuteList(NULL, NULL);
            }
            else
            {
                MechaCommandListClear();
                result = 0;
            }
        }
        else
        {
            PlatShowMessage("An error occurred. Wrong chassis selected?, result = %d\n", result);
        }

        return result;
    }
    else
    {
        PlatShowMessage("Unsupported chassis selected.\n");
        return -EINVAL;
    }
}

static int SelectChassis(void)
{
    typedef int (*ChassisProbe_t)(void);
    struct ChassisData
    {
        ChassisProbe_t probe;
        const char *label;
    };
    struct ChassisData data[MECHA_CHASSIS_MODEL_COUNT] = {
        {&IsChassisCex10000, "A-chassis (SCPH-10000/SCPH-15000, GH-001/3)"},
        {&IsChassisA, "A-chassis (SCPH-15000/SCPH-18000+ with TI RF-AMP, GH-003)"},
        {&IsChassisB, "AB-chassis (SCPH-18000, GH-008)"},
        {&IsChassisB, "B-chassis (SCPH-30001 with Auto-Tilt motor)"},
        {&IsChassisC, "C-chassis (SCPH-30001/2/3/4)"},
        {&IsChassisD, "D-chassis (SCPH-300xx/SCPH-350xx)"},
        {&IsChassisF, "F-chassis (SCPH-30000/SCPH-300xx R)"},
        {&IsChassisG, "G-chassis (SCPH-390xx)"},
        {&IsChassisDragon, "Dragon (SCPH-5x0xx--SCPH-900xx)"},
        {&IsChassisDexA, "A-chassis (DTL-H10000)"},  // A
        {&IsChassisDexA, "A-chassis (DTL-T10000H)"}, // A2
        {&IsChassisDexA, "A-chassis (DTL-T10000)"},  // A3
        {&IsChassisDexB, "B-chassis (DTL-H30001/2 with Auto-Tilt motor)"},
        {&IsChassisDexD, "D-chassis (DTL-H30x0x)"},
        {&IsChassisDragon, "Dragon (DTL-5x0xx--DTL-900xx)"}};
    int SelectCount, LastSelectIndex, i, choice;

    DisplayCommonConsoleInfo();
    PlatShowMessage("Chassis:\n");
    for (i = 0, SelectCount = 0, LastSelectIndex = -1; i < MECHA_CHASSIS_MODEL_COUNT; i++)
    {
        if (data[i].probe() != 0)
        {
            PlatShowMessage("\t%2d. %s\n", i + 1, data[i].label);
            SelectCount++;
            LastSelectIndex = i;
        }
    }

    if (SelectCount > 1)
    {
        PlatShowMessage("\t%2d. None\n", i + 1);
        do
        {
            PlatShowMessage("Choice: ");
            choice = 0;
            if (scanf("%d", &choice) > 0)
                while (getchar() != '\n')
                {
                };
        } while (choice < 1 || choice > i + 1);

        --choice;
        if (choice == i)
            choice = -1;
    }
    else
    {
        choice = LastSelectIndex;
    }

    return choice;
}

void MenuEEPROM(void)
{
    static const char *ChassisNames[MECHA_CHASSIS_MODEL_COUNT] = {
        "A-chassis (SCPH-10000/SCPH-15000, GH-001/3)",
        "A-chassis (SCPH-15000/SCPH-18000+ w/ TI RF-AMP, GH-003)",
        "AB-chassis (SCPH-18000, GH-008)",
        "B-chassis (SCPH-30001 with Auto-Tilt motor)",
        "C-chassis",
        "D-chassis",
        "F-chassis",
        "G-chassis (SCPH-3900x)",
        "H-chassis",
        "A-chassis (DTL-H10000)",
        "A-chassis (DTL-T10000H)",
        "A-chassis (DTL-T10000)",
        "B-chassis (DTL-H30001/2)",
        "D-chassis (DTL-H30000)",
        "H-chassis (DTL-H500xx)"};
    unsigned char done;
    short int choice, chassis = -1;
    char filename[256];

    done = 0;
    do
    {
        if (MechaInitModel() != 0)
        {
            DisplayConnHelp();
            return;
        }
        if (IsOutdatedBCModel())
            PlatShowMessage("B/C-chassis: EEPROM update required.\n");
        if (chassis < 0)
            chassis = SelectChassis();
        do
        {
            PlatShowMessage("\nSelected chassis: %s\n"
                            "EEPROM operations:\n"
                            "\t1. Display console information\n"
                            "\t2. Dump EEPROM\n"
                            "\t3. Restore EEPROM\n"
                            "\t4. Erase EEPROM\n"
                            "\t5. Load defaults (All)\n"
                            "\t6. Load defaults (Disc Detect)\n"
                            "\t7. Load defaults (Servo)\n"
                            "\t8. Load defaults (Tilt)\n"
                            "\t9. Load defaults (Tray)\n"
                            "\t10. Load defaults (EEGS)\n"
                            "\t11. Load defaults (OSD)\n"
                            "\t12. Load defaults (RTC)\n"
                            "\t13. Load defaults (DVD Player)\n"
                            "\t14. Load defaults (ID)\n"
                            "\t15. Load defaults (Model Name)\n"
                            "\t16. Load defaults (SANYO OP)\n"
                            "\t17. Update EEPROM\n"
                            "\t18. Quit\n"
                            "\nYour choice: ",
                            chassis < 0 ? "Unknown" : ChassisNames[chassis]);
            choice = 0;
            if (scanf("%hd", &choice) > 0)
                while (getchar() != '\n')
                {
                };
        } while (choice < 1 || choice > 18);

        switch (choice)
        {
            case 1:
                DisplayCommonConsoleInfo();
                PlatShowMessage("Press ENTER to continue\n");
                while (getchar() != '\n')
                {
                };
                break;
            case 2:
                char useDefault;
                char default_filename[256];
                u32 serial = 0;
                u8 emcs    = 0;

                if (EEPROMInitSerial() == 0)
                    EEPROMGetSerial(&serial, &emcs);

                const char *model = EEPROMGetModelName();

                const struct MechaIdentRaw *RawData;
                RawData = MechaGetRawIdent();

                // Format the filename
                snprintf(default_filename, sizeof(default_filename), "%s_%07u_%s_%#08x.bin", model, serial, RawData->cfd, RawData->cfc);

                PlatShowMessage("Default filename: %s\n", default_filename);
                PlatShowMessage("Do you want to use the default filename? (Y/N): ");

                if (scanf(" %c", &useDefault) > 0)
                    while (getchar() != '\n')
                    {
                    };

                if (useDefault == 'Y' || useDefault == 'y')
                    strcpy(filename, default_filename);
                else
                {
                    PlatShowMessage("Enter dump filename: ");
                    if (fgets(filename, sizeof(filename), stdin))
                        filename[strlen(filename) - 1] = '\0';
                }

                PlatShowMessage("Dump %s.\n", DumpEEPROM(filename) == 0 ? "completed" : "failed");
                break;
            case 3:
                PlatShowMessage("Enter dump filename: ");
                if (fgets(filename, sizeof(filename), stdin))
                {
                    filename[strlen(filename) - 1] = '\0';
                    // gets(filename);
                    PlatShowMessage("Restore %s.\n", RestoreEEPROM(filename) == 0 ? "completed" : "failed");
                }
                break;
            case 4:
#ifdef ID_MANAGEMENT
                PlatShowMessage("EEPROM erase %s.\n", EEPROMClear() == 0 ? "completed" : "failed");
#else
                PlatShowMessage("Function disabled.\n");
#endif
                break;
            case 5:
#ifdef ID_MANAGEMENT
                PlatShowMessage("Defaults (all) load: %s.\n", EEPROMDefaultAll() == 0 ? "completed" : "failed");
#else
                PlatShowMessage("Function disabled.\n");
#endif
                break;
            case 6:
                PlatShowMessage("Defaults (disc detect) load: %s.\n", EEPROMDefaultDiscDetect() == 0 ? "completed" : "failed");
                break;
            case 7:
                PlatShowMessage("Defaults (servo) load: %s.\n", EEPROMDefaultServo() == 0 ? "completed" : "failed");
                break;
            case 8:
                PlatShowMessage("Defaults (tilt) load: %s.\n", EEPROMDefaultTilt() == 0 ? "completed" : "failed");
                break;
            case 9:
                PlatShowMessage("Defaults (tray) load: %s.\n", EEPROMDefaultTray() == 0 ? "completed" : "failed");
                break;
            case 10:
                PlatShowMessage("Defaults (EEGS) load: %s.\n", EEPROMDefaultEEGS() == 0 ? "completed" : "failed");
                break;
            case 11:
                PlatShowMessage("Defaults (OSD) load: %s.\n", EEPROMDefaultOSD() == 0 ? "completed" : "failed");
                break;
            case 12:
                PlatShowMessage("Defaults (RTC) load: %s.\n", EEPROMDefaultRTC() == 0 ? "completed" : "failed");
                break;
            case 13:
                PlatShowMessage("Defaults (DVD Player) load: %s.\n", EEPROMDefaultDVDVideo() == 0 ? "completed" : "failed");
                break;
            case 14:
#ifdef ID_MANAGEMENT
                PlatShowMessage("Defaults (ID) load: %s.\n", EEPROMDefaultID() == 0 ? "completed" : "failed");
#else
                PlatShowMessage("Function disabled.\n");
#endif
                break;
            case 15:
#ifdef ID_MANAGEMENT
                PlatShowMessage("Defaults (Model Name) load: %s.\n", EEPROMDefaultModelName() == 0 ? "completed" : "failed");
#else
                PlatShowMessage("Function disabled.\n");
#endif
                break;
            case 16:
                PlatShowMessage("Defaults (Sanyo OP) load: %s.\n", EEPROMDefaultSanyoOP() == 0 ? "completed" : "failed");
                break;
            case 17:
                PlatShowMessage("EEPROM update: %s.\n", UpdateEEPROM(chassis) == 0 ? "completed" : "failed");
                break;
            case 18:
                done = 1;
                break;
        }

        PlatShowMessage("\nIf the EEPROM was updated, please reboot the MECHACON\n"
                        "by leaving this menu before pressing the RESET button.\n");
    } while (!done);
}
