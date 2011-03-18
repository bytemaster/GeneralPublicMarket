#ifndef _CLIENT_STATE_HPP_
#define _CLIENT_STATE_HPP_




/**
 *  @class ClientState
 *
 *  This class is responsible for managing the client's state which includes the following:
 *
 *  map<public_key,private_key> all known public/private keys encrypted in a database.
 *  map<keyname, pair<public_key,private_key> >  all names for which we have a private key
 *
 *  The user will have the freedom to manage their names.  Any user can view any account, but
 *  without the private key they can do nothing.   An icon will indicate whether or not an account
 *  is locked and whether or not we have a key for it.
 *
 *
 *  Accounts
 *
 *  Group Name
 *    
 *
 *
 *
 *
 *
 *
 *
 */
class ClientState
{


};


#endif
