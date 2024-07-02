/** @file Move the stack of patient QA folders in CWD/patients to the remote
 *      folder supplied... somehow
 *
 *  I'm just going to hardcode the iteration, it's not worth linking to
 *  qagen-files.c because it has dependencies on C++ translation units
 */
#include "src/qagen-defs.h"
#include "src/qagen-files.h"
#include <stdio.h>


static int patient_dir_callback(const PATH      *path,
                                WIN32_FIND_DATA *fdata,
                                void            *data)
{
    _putws(path->buf);
    return 0;
}


static const wchar_t *patient_qa_dir(void)
{
    return L"\\\\win.ad.jhu.edu\\cloud\\radonc$\\hdrive\\Clinical Sites\\Dosimetry\\Patient QA\\";
}


int wmain(int argc, wchar_t *argv[])
{
    const wchar_t *dst;
    PATH *pt;

    pt = qagen_path_create(L".\\patients");
    qagen_file_foreach(pt, L"*", patient_dir_callback, NULL);
    return 0;
}
