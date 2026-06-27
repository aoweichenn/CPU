#include <cse/reference_pipeline.hpp>

#include <iostream>

int main()
{
    const cse::InputCase input{
        "normal_5_lines",
        1,
        "view,1\nclick,1\nview,2\nlogin,1\nlogout,1\n",
    };

    const cse::ReferenceResult result = cse::run_reference(input);
    std::cout << cse::format_report(result);
    std::cout << "manifest=" << cse::format_manifest_row(cse::make_manifest_row(result)) << '\n';
    return 0;
}
