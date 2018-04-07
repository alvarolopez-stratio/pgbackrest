/***********************************************************************************************************************************
Test Debug Macros and Routines when Disabled
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("DEBUG"))
    {
#ifdef DEBUG
        bool debug = true;
#else
        bool debug = false;
#endif

        TEST_RESULT_BOOL(debug, false, "DEBUG is not defined");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("DEBUG_UNIT_EXTERN"))
    {
        const char *debugUnitExtern = "" DEBUG_UNIT_EXTERN "";
        TEST_RESULT_STR(debugUnitExtern, "", "DEBUG_UNIT_EXTERN is static");
    }
}