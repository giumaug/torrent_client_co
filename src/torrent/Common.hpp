#include <string>

constexpr auto wrong_payload = "Wrong payload.";
constexpr auto connect_failed = "Open session failed.";
constexpr auto close_failed = "Close session failed.";
constexpr auto read_error = "Read error.";
constexpr auto write_error = "Write error.";
constexpr auto handshake_error = "Handshake error.";
constexpr auto file_error = "File error.";
constexpr auto generic_error = "Generic error.";
constexpr auto announce_error = "Announce error.";
constexpr auto no_peer_found_error = "No peer found error.";

enum EX_CODE
{
	WRONG_PAYLOAD,
	OPEN_SESSION_FAILED,
  CLOSE_SESSION_FAILED,
	READ_ERROR,
	WRITE_ERROR,
	HANDSHAKE_ERROR,
  ANNOUNCE_FAILED,
  PARSE_FAILED,
  FILE_ERROR,
  NO_PEER_FOUND,
	GENERIC_ERROR,
};

class TorrentException {
  public:
    TorrentException (EX_CODE _ex) : ex(_ex) {};
    ~TorrentException (){};
    std::string description() {
	  std::string errorDesc;	 
        switch(ex) {
          case WRONG_PAYLOAD:
            errorDesc = wrong_payload;
            break;
          case OPEN_SESSION_FAILED:
            errorDesc = connect_failed;
            break;
          case CLOSE_SESSION_FAILED:
            errorDesc = close_failed;
            break;
          case READ_ERROR:
            errorDesc = read_error;
            break;
          case WRITE_ERROR:
            errorDesc = write_error;
            break;
          case HANDSHAKE_ERROR:
            errorDesc = handshake_error;
            break;
          case ANNOUNCE_FAILED:
            errorDesc = announce_error;
            break;
          case FILE_ERROR:
            errorDesc = file_error;
            break;
          case NO_PEER_FOUND:
            errorDesc = no_peer_found_error;
            break;  
          default:
            errorDesc = generic_error;
        }
        return errorDesc;
    }
  private:
    EX_CODE ex;
};

