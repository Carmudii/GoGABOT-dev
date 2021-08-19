#include "gt.hpp"
#include "packet.h"
#include "server.h"
#include "utils.h"

using namespace std;

/************************** Default configuration **************************/
string gt::configuration_file_name = ".config.bin";
string gt::version                 = "3.67";
string gt::flag                    = "ch";
string gt::target_world_name       = "";
string gt::owner_name              = "";
string gt::bot_name                = "";
string gt::bot_password            = "";
string gt::spam_text               = "";

int gt::owner_net_id               = 0;
int gt::spam_delay                 = 3500;
int gt::hit_per_block              = 1;
int gt::safety_spam                = 0;

bool gt::connecting                = false;
bool gt::in_game                   = false;
bool gt::is_following_owner        = false;
bool gt::is_following_public       = false;
bool gt::is_following_punch        = false;
bool gt::is_auto_ban               = false;
bool gt::is_spam_active            = false;
bool gt::is_auto_message_active    = false;
bool gt::is_auto_break_active      = false;
bool gt::is_owner_command          = false;
bool gt::is_exit                   = false;
bool gt::is_activated_auto_message = false;
/************************** Default configuration **************************/

void gt::solve_captcha(std::string text)
{
    //Get the sum :D
    utils::replace(text,
                   "set_default_color|`o\nadd_label_with_icon|big|`wAre you Human?``|left|206|\nadd_spacer|small|\nadd_textbox|What will be the sum of the following "
                   "numbers|left|\nadd_textbox|",
                   "");
    utils::replace(text, "|left|\nadd_text_input|captcha_answer|Answer:||32|\nend_dialog|captcha_submit||Submit|", "");
    auto number1 = text.substr(0, text.find(" +"));
    auto number2 = text.substr(number1.length() + 3, text.length());
    int result = atoi(number1.c_str()) + atoi(number2.c_str());
    g_server->send("action|dialog_return\ndialog_name|captcha_submit\ncaptcha_answer|" + to_string(result));
}
