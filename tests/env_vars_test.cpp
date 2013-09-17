/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Contributed by: Intel Corporation
 */

/* ===
 *
 * Tests for environment variables utility functions in Bionic libc.
 *
 * Functions covered:
 *   getenv()   - get value of env-var
 *   setenv()   - set value of env-var
 *   putenv()   - set env-var value by tuple
 *   unsetenv() - dispose env-var
 *   clearenv() - clear env-var set
 */

#include "snippets.h"

#include <string>
#include <vector>

#include <errno.h>
#include <sys/wait.h>

const static std::string ENV_VAR     = "LIBC_TEST_ENV_VAR";
const static std::string ENV_NOT_VAR = "LIBC_TEST_NOT_ENV_VAR";
const static std::string ENV_VAL     = "Some dummy string from outer space";
const static std::string ENV_VAL_B   = "Yet another dummy string";

/*
 * This test checks that
 *   getenv() works,
 *   getenv() returns NULL on non-existent var.
 */
TEST(envvars, getenv)
{
    CRASH_CATCH();

    char *env;

    { SCOPED_TRACE("getenv() init");
        // Populate ENV_VAR
        setenv(ENV_VAR.c_str(), ENV_VAL.c_str(), 1);
        // Dispose ENV_NOT_VAR
        unsetenv(ENV_NOT_VAR.c_str());
    }

    { SCOPED_TRACE("getenv()");
        // ENV_VAR is already populated by setenv()
        env = getenv(ENV_VAR.c_str());
        ASSERT_TRUE(env); // Check for NULL
        EXPECT_FALSE(strcmp(ENV_VAL.c_str(), env));
    }

    { SCOPED_TRACE("getenv() non-existent var");
        // ENV_NOT_VAR is already disposed by unsetenv()
        env = getenv(ENV_NOT_VAR.c_str());
        ASSERT_FALSE(env); // NULL is expected
    }

    // Cleaning up
    unsetenv(ENV_VAR.c_str());
}

/*
 * This test checks that
 *   setenv() works correctly in
 *     new var,
 *     no-rewrite,
 *     rewrite
 *   cases.
 */
TEST(envvars, setenv)
{
    CRASH_CATCH();

    char *env;

    { SCOPED_TRACE("setenv() new var");
        // ENV_VAR is already disposed by unsetenv()
        ASSERT_FALSE(setenv(ENV_VAR.c_str(), ENV_VAL_B.c_str(), 0));

        env = getenv(ENV_VAR.c_str());
        ASSERT_TRUE(env); // Check for NULL
        EXPECT_FALSE(strcmp(ENV_VAL_B.c_str(), env));
    }

    { SCOPED_TRACE("setenv() no-rewrite");
        // 0 means no-rewrite
        ASSERT_FALSE(setenv(ENV_VAR.c_str(), ENV_VAL.c_str(), 0));

        env = getenv(ENV_VAR.c_str());
        ASSERT_TRUE(env); // Check for NULL

        // ENV_VAL_B should NOT be rewritten by ENV_VAL
        EXPECT_FALSE(strcmp(ENV_VAL_B.c_str(), env));
    }

    { SCOPED_TRACE("setenv() rewrite");
        // 1 means rewrite
        ASSERT_FALSE(setenv(ENV_VAR.c_str(), ENV_VAL.c_str(), 1));

        env = getenv(ENV_VAR.c_str());
        ASSERT_TRUE(env); // Check for NULL

        // ENV_VAL rewrites ENV_VAL_B
        EXPECT_FALSE(strcmp(ENV_VAL.c_str(), env));
    }

    // Cleaning up
    unsetenv(ENV_VAR.c_str());
}

/*
 * This test checks that
 *   putenv() works,
 *   putenv() rewrites existant var.
 */
TEST(envvars, putenv)
{
    CRASH_CATCH();

    char *env;
    std::string ENV_TUPLE   = ENV_VAR + "=" + ENV_VAL;
    std::string ENV_TUPLE_B = ENV_VAR + "=" + ENV_VAL_B;

    { SCOPED_TRACE("putenv() init cleanup");
        // Redurant but related to test
        unsetenv(ENV_VAR.c_str());
        env = getenv(ENV_NOT_VAR.c_str());
        ASSERT_FALSE(env); // NULL is expected
    }

    { SCOPED_TRACE("putenv()");
        // ENV_VAR is already disposed by unsetenv()
        ASSERT_FALSE(putenv(ENV_TUPLE.c_str()));

        env = getenv(ENV_VAR.c_str());
        ASSERT_TRUE(env); // Check for NULL
        EXPECT_FALSE(strcmp(ENV_VAL.c_str(), env));
    }

    { SCOPED_TRACE("putenv() rewrite");
        // putenv() should rewrite ENV_VAR
        ASSERT_FALSE(putenv(ENV_TUPLE_B.c_str()));

        env = getenv(ENV_VAR.c_str());
        ASSERT_TRUE(env); // Check for NULL
        EXPECT_FALSE(strcmp(ENV_VAL_B.c_str(), env));
    }

    // Cleaning up
    unsetenv(ENV_VAR.c_str());
}

/*
 * This test checks that
 *   unsetenv() works.
 */
TEST(envvars, unsetenv)
{
    CRASH_CATCH();

    char *env;

    { SCOPED_TRACE("unsetenv() init populate");
        // Writes ENV_VAR
        ASSERT_FALSE(setenv(ENV_VAR.c_str(), ENV_VAL.c_str(), 1));

        env = getenv(ENV_VAR.c_str());
        ASSERT_TRUE(env); // Check for NULL
        EXPECT_FALSE(strcmp(ENV_VAL.c_str(), env));
    }

    { SCOPED_TRACE("unsetenv()");
        // Clears ENV_VAR
        ASSERT_FALSE(unsetenv(ENV_VAR.c_str()));

        env = getenv(ENV_NOT_VAR.c_str());
        ASSERT_FALSE(env); // NULL is expected
    }
}

/*
 * This test checks that clearenv() works.
 *
 * NOTE: Env vars set should be restored after test.
 */
TEST(envvars, clearenv)
{
    CRASH_CATCH();

    extern char **environ; // System env vars
    std::vector<std::string> env_stor; // Temporary env vars storage

    { SCOPED_TRACE("clearenv() init populate");
        // To guarantee that env vars set is not initially empty
        setenv(ENV_VAR.c_str(), ENV_VAL.c_str(), 1);
        setenv(ENV_NOT_VAR.c_str(), ENV_VAL_B.c_str(), 1);
    }

    { SCOPED_TRACE("clearenv() backup");
        int k;
        for (char *var = environ[k=0]; var != NULL; var = environ[++k])
        {
            env_stor.push_back(var);
        }
    }

    { SCOPED_TRACE("clearenv()");
        // Clears env vars set
        EXPECT_FALSE(clearenv());

        // Checking emptyness
        ASSERT_FALSE(*environ);
    }

    { SCOPED_TRACE("clearenv() restore");
        // Restoring
        for (unsigned int k = 0; k < env_stor.size(); ++k)
        {
            putenv(env_stor[k].c_str());
        }
        // Checking restored env
        char *env = getenv(ENV_VAR.c_str());
        ASSERT_TRUE(env); // Check for NULL
        EXPECT_FALSE(strcmp(ENV_VAL.c_str(), env));
    }

    // Cleaning up
    unsetenv(ENV_VAR.c_str());
    unsetenv(ENV_NOT_VAR.c_str());
}
