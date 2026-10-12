//*****************************************************************************
//
// DxSystem例外
//
// DxSystemException.h
//
// K_Yamaoka
//
// 2016/11/18
//
//*****************************************************************************

#pragma once

constexpr const wchar_t* WINDOW_CLASS_NAME = L"3DSystemWindow";

class DxSystemException
{
public:

	//オリジナルエラーメッセージ列挙体
	enum OriginalMessage {
		OM_UNKNOWN_ERROR,                        //予期せぬエラーが発生
		OM_WINDOW_INITIALIZE_ERROR,              //＄ウィンドウクラスの初期化に失敗
		OM_GAME_INITIALIZE_ERROR,                //＄ゲームクラスの初期化に失敗
		OM_D3D_CREATE_ERROR,                     //＄Ｄｉｒｅｃｔ３Ｄ本体の作成に失敗
		OM_DEVICE_CREATE_ERROR,                  //＄デバイスの作成に失敗
		OM_SPRITE_CREATE_ERROR,                  //＄スプライトの作成に失敗
		OM_TEXTUREMANAGE_ALLOCATE_ERROR,         //＄テクスチャ管理クラスの領域確保に失敗
		OM_TEXTUREMANAGE_INITIALIZE_ERROR,       //＄テクスチャ管理クラスの初期化に失敗
		OM_DEVICE_CLEAR_ERROR,                   //＄画面のクリアに失敗
		OM_DEVICE_BEGINSCENE_ERROR,              //＄描画の開始に失敗
		OM_DEVICE_ALPHA_ENABLE_ERROR,            //＄アルファブレンドの設定に失敗
		OM_DEVICE_RENDER_SRC_ERROR,              //＄転送元アルファの設定に失敗
		OM_DEVICE_RENDER_DST_ERROR,              //＄転送先アルファの設定に失敗
		OM_ZWRITEDISABLE_ERROR,                  //＄Ｚバッファへの書き込み禁止に失敗
		OM_SPRITE_BEGIN_ERROR,                   //＄スプライトの描画開始に失敗
		OM_SPRITE_END_ERROR,                     //＄スプライトの描画終了に失敗
		OM_ZWRITEENABLE_ERROR,                   //＄Ｚバッファへの書き込み許可に失敗
		OM_DEVICE_ALPHA_DISABLE_ERROR,           //＄アルファブレンドの無効化に失敗
		OM_DEVICE_ENDSCENE_ERROR,                //＄描画の終了に失敗
		OM_DEVICE_PRESENT_ERROR,                 //＄フロントバッファへの転送に失敗
		OM_DEVICE_GETBACKBUFFER_ERROR,           //バックバッファの取得に失敗
		OM_LOCK_BACKBUFFER_ERROR,                //バックバッファのロックに失敗
		OM_TEXTURE_CREATE_ERROR,                 //＄テクスチャの読み込みに失敗
		OM_TEXTURE_ALLOCATE_ERROR,               //＄テクスチャクラスの領域確保に失敗
		//OM_TEXTUREMANAGE_ADDTEXTURE_ERROR,       //＄テクスチャの登録に失敗
		OM_D3D_GETCAPS_ERROR,                    //＄デバイスの能力取得に失敗
		OM_TEXTURE_WIDTH_OVER_ERROR,             //＄テクスチャの幅が最大値を超えています
		OM_TEXTURE_HEIGHT_OVER_ERROR,            //＄テクスチャの高さが最大値を超えています
		OM_TEXTURE_DIFFERENT_SIZE_ERROR,         //＄テクスチャの幅と高さが違います
		OM_TEXTURE_SIZE_ERROR,                   //＄テクスチャのサイズが２の累乗ではありません
		//OM_TEXTURE_RELEASE_ERROR,                //＄テクスチャの解放に失敗
		OM_WINDOW_COMMANDLINE_ERROR,             //＄コマンドライン引数は使用できません
		OM_WINDOW_REGIST_ERROR,                  //＄ウィンドウの登録に失敗
		OM_WINDOW_CREATE_ERROR,                  //＄ウィンドウの作成に失敗
		OM_TEXTUREMANAGE_NOTFOUND_ERROR,         //＄テクスチャが見つかりません
		OM_SPRITE_DRAW_ERROR,                    //＄スプライトの描画に失敗
		//OM_FONTMANAGE_RELOAD_ERROR,              //フォントの再作成に失敗
		OM_FONTMANAGE_ALLOCATE_ERROR,            //＄フォント管理クラスの領域確保に失敗
		OM_FONT_CREATE_ERROR,                    //＄フォントの作成に失敗
		OM_FONT_ALLOCATE_ERROR,                  //＄フォントの領域確保に失敗
		//OM_FONTMANAGE_ADDFONT_ERROR,             //＄フォントの登録に失敗
		//OM_FONT_RELEASE_ERROR,                   //＄フォントの解放に失敗
		OM_DRAWSTRING_ERROR,                     //＄文字列の表示に失敗
		OM_FPSMANAGE_ALLOCATE_ERROR,             //＄ＦＰＳ管理クラスの領域確保に失敗
		OM_FONTMANAGE_NOTFOUND_ERROR,            //＄フォントが見つかりません
		//OM_MESSAGE_ALLOCATE_ERROR,               //メッセージクラスの領域確保に失敗
		//OM_MESSAGE_FILENOTOPEN_ERROR,            //メッセージ用ファイルが見つかりません
		OM_SS_CANTCREATE_OFFSCREENSURFACE_ERROR, //＄オフスクリーンサーフェス作成に失敗
		OM_SS_CANTGET_FRONTBUFFER_ERROR,         //＄フロントバッファの取得に失敗
		OM_SS_CANT_SAVE_ERROR,                   //＄スクリーンショットの保存に失敗
		OM_BGM_NAME_ALLOCATE_ERROR,              //＄ＢＧＭファイル名の領域取得に失敗
		OM_BGM_CREATE_GRAPHBUILDER_ERROR,        //＄グラフビルダーの作成に失敗
		OM_BGM_CREATE_MEDIACONTROL_ERROR,        //＄メディアコントロールの作成に失敗
		OM_BGM_CREATE_MEDIAEVENT_ERROR,          //＄メディアイベントの作成に失敗
		OM_BGM_CREATE_MEDIASEEKING_ERROR,        //＄メディアシーキングの作成に失敗
		OM_BGM_CREATE_FILTERGRAPH_ERROR,         //＄フィルタグラフの作成に失敗
		OM_BGM_CREATE_BASICAUDIO_ERROR,          //＄ベーシックオーディオの作成に失敗
		OM_BGM_PLAY_ERROR,                       //＄ＢＧＭの再生に失敗
		OM_BGM_STOP_ERROR,                       //＄ＢＧＭの停止に失敗
		OM_BGMMANAGE_ALLOCATE_ERROR,             //＄ＢＧＭ管理クラスの領域確保に失敗
		OM_INITCOM_ERROR,                        //＄ＣＯＭの初期化に失敗
		OM_BGMMANAGE_INITIALIZE_ERROR,           //＄ＢＧＭ管理クラスの初期化に失敗
		//OM_NOT_INITIALIZED_ERROR,                //クラスの初期化が行われていません
		OM_SE_CREATE_PERFORMANCE_ERROR,          //＄パフォーマンスの作成に失敗
		OM_SE_INIT_AUDIO_ERROR,                  //＄オーディオの初期化に失敗
		OM_SE_CREATE_LOADER_ERROR,               //＄ローダーの作成に失敗
		OM_SE_GETCURRENTDIR_ERROR,               //＄カレントディレクトリの取得に失敗
		OM_SE_SETSEARCHDIR_ERROR,                //＄ローダーへのディレクトリ登録に失敗
		OM_SE_LOAD_ERROR,                        //＄ＳＥのロードに失敗
		OM_SE_DOWNLOAD_ERROR,                    //＄バンドのダウンロードに失敗
		OM_SE_ALLOCATE_ERROR,                    //＄ＳＥクラスの領域確保に失敗
		OM_SE_CREATE_DIRECTSOUND_ERROR,          //DirectSoundの作成に失敗
		OM_SE_SET_COOPERATIVELEVEL_ERROR,        //協調レベルの設定に失敗
		OM_SE_SET_SPEEKER_ERROR,                 //スピーカーの設定に失敗
		OM_SE_CREATE_PRIMARYBUFFER_ERROR,        //プライマリバッファの作成に失敗
		OM_SE_ALLOC_FILENAME_ERROR,              //Weveファイル名用領域確保に失敗
		OM_SE_WAVE_FILE_OPEN_ERROR,              //Weveファイルのオープンに失敗
		OM_SE_DESCEND_DATACHUNK_ERROR,           //データチャンクへの進入に失敗
		OM_SE_CREATE_SECONDARYBUFFER_ERROR,      //セカンダリバッファの作成に失敗
		OM_SE_PLAY_ERROR,                        //＄ＳＥの再生に失敗
		OM_SE_STOP_ERROR,                        //＄ＳＥの停止に失敗
		OM_SEMANAGE_ALLOCATE_ERROR,              //＄ＳＥ管理クラスの領域確保に失敗
		OM_SEMANAGE_INITIALIZE_ERROR,            //＄ＳＥ管理クラスの初期化に失敗
		OM_NUMBER_DIGITSOVER_ERROR,              //＄表示したい桁数より数値が大きいです
		OM_NUMBER_ALLOCATE_ERROR,                //＄桁別数値格納領域の確保に失敗
		OM_BB_CREATE_VB_ERROR,                   //＄バウンディングボックス用頂点バッファの作成に失敗
		OM_BB_CREATE_IB_ERROR,                   //＄バウンディングボックス用インデックスバッファの作成に失敗
		OM_BB_LOCK_VB_ERROR,                     //＄バウンディングボックス用頂点バッファのロックに失敗
		OM_BB_LOCK_IB_ERROR,                     //＄バウンディングボックス用インデックスバッファのロックに失敗
		OM_BS_CREATE_VB_ERROR,                   //＄バウンディングスフィア用頂点バッファの作成に失敗
		OM_BS_CREATE_IB_ERROR,                   //＄バウンディングスフィア用インデックスバッファの作成に失敗
		OM_BS_LOCK_VB_ERROR,                     //＄バウンディングスフィア用頂点バッファのロックに失敗
		OM_BS_LOCK_IB_ERROR,                     //＄バウンディングスフィア用インデックスバッファのロックに失敗
		OM_LIGHT_AMBIENT_SETDEVICE_ERROR,        //＄アンビエントライトのデバイス登録に失敗
		//OM_LIGHT_DIRECTIONAL_SETDEVICE_ERROR,    //＄ディレクショナルライトのデバイス登録に失敗
		OM_LIGHT_DIRECTIONAL_DISABLE_ERROR,      //＄ディレクショナルライトの消灯に失敗
		OM_LIGHT_DIRECTIONAL_ENABLE_ERROR,       //＄ディレクショナルライトの点灯に失敗
		OM_LIGHT_POINT_SETDEVICE_ERROR,          //＄ポイントライトのデバイス登録に失敗
		OM_LIGHT_POINT_DISABLE_ERROR,            //＄ポイントライトの消灯に失敗
		OM_LIGHT_POINT_ENABLE_ERROR,             //＄ポイントライトの点灯に失敗
		OM_LIGHT_SPOT_SETDEVICE_ERROR,           //＄スポットライトのデバイス登録に失敗
		OM_LIGHT_SPOT_DISABLE_ERROR,             //＄スポットライトの消灯に失敗
		OM_LIGHT_SPOT_ENABLE_ERROR,              //＄スポットライトの点灯に失敗
		OM_CAMERA_SETDEVICE_ERROR,               //＄カメラのデバイス登録に失敗
		OM_PROJECTION_SETDEVICE_ERROR,           //＄プロジェクションのデバイス登録に失敗
		OM_VIEWPORT_SETDEVICE_ERROR,             //＄ビューポートのデバイス登録に失敗
		OM_PRIMITIVE_CREATE_MESH_ERROR,          //プリミティブ用メッシュの作成に失敗
		OM_PRIMITIVE_LOCK_VB_ERROR,              //プリミティブ用頂点バッファのロックに失敗
		OM_PRIMITIVE_LOCK_IB_ERROR,              //プリミティブ用インデックスバッファのロックに失敗
		OM_PRIMITIVE_CREATE_SPHERE_ERROR,        //球体の作成に失敗
		OM_PRIMITIVE_COMPUTE_NORMAL_ERROR,       //球体の法線計算に失敗
		OM_PRIMITIVE_CLONE_FVF_ERROR,            //球体のクローン作成に失敗
		OM_PRIMITIVE_LOAD_TEXTURE_ERROR,         //プリミティブ用テクスチャの読み込みに失敗
		OM_PARTICLE_LOAD_TEXTURE_ERROR,          //パーティクル用テクスチャの読み込みに失敗
		OM_MESH_LOAD_ERROR,                      //Ｘファイルのロードに失敗
		OM_MESH_BONEMATRIX_SETUP_ERROR,          //メッシュのボーンマトリックス設定に失敗
		//OM_MESH_INIT_ANIMATIONSET_ERROR,         //メッシュのアニメーションセット設定に失敗　※場所変更
		OM_MESH_GETMATRIX_ROOTFRAME_ERROR,       //ルートフレームの行列取得に失敗
		OM_MESH_GETMATRIX_FRAMENAME_ERROR,       //指定したフレームの行列取得に失敗
		OM_MESH_ALOC_ADJACENCYDATA_ERROR,        //隣接データ構造体配列の領域確保に失敗
		OM_MESH_ALOC_ADJACENCY_ERROR,            //隣接データ配列の領域確保に失敗
		OM_MESH_GENERATE_ADJACENCY_ERROR,        //隣接データの生成に失敗
		OM_MESH_LOCK_VB_ERROR,                   //メッシュの頂点バッファロックに失敗
		OM_MESH_LOCK_IB_ERROR,                   //メッシュのインデックスバッファロックに失敗
		OM_BILLBOARD_CREATE_MESH_ERROR,          //ビルボード用メッシュの作成に失敗
		OM_BILLBOARD_LOCK_VB_ERROR,              //ビルボードの頂点バッファロックに失敗
		OM_BILLBOARD_LOCK_IB_ERROR,              //ビルボードのインデックスバッファロックに失敗
		OM_BILLBOARD_LOAD_TEXTURE_ERROR,         //ビルボード用テクスチャの読み込みに失敗
		OM_FRAME_SETUPBONEMATRIX_ERROR,          //フレームへのボーンマトリックス設定に失敗
		OM_FRAME_SETUPBONEMATRIX_SIBLING_ERROR,  //兄弟フレームへのボーンマトリックス設定に失敗
		OM_FRAME_SETUPBONEMATRIX_CHILD_ERROR,    //子フレームへのボーンマトリックス設定に失敗
		OM_FRAME_SETTRANSFORM_ERROR,             //変換行列の設定に失敗
		OM_FRAME_VERTEXBLEND_ON_ERROR,           //頂点ブレンドの設定に失敗
		OM_FRAME_SETMATERIAL_ERROR,              //マテリアルの設定に失敗
		OM_FRAME_SETTEXTURE_ERROR,               //テクスチャの設定に失敗
		OM_FRAME_DRAWSUBSET_ERROR,               //サブセットの描画に失敗
		OM_FRAME_SOFT_VERTEX_PROCESS_ON_ERROR,   //ソフトウェア頂点処理の開始に失敗
		OM_FRAME_SOFT_VERTEX_PROCESS_OFF_ERROR,  //ソフトウェア頂点処理の終了に失敗
		OM_FRAME_VERTEXBLEND_OFF_ERROR,          //頂点ブレンドの終了に失敗
		OM_AH_CREATE_FRAME_DERIVED_ERROR,        //拡張フレームの領域確保に失敗
		OM_AH_COPYFRAMENAME_ERROR,               //フレーム名のコピーに失敗
		OM_AH_MESH_TYPE_ERROR,                   //ノーマルタイプのメッシュのみ利用できます
		OM_AH_NONE_FVF_ERROR,                    //ＦＶＦが設定されていないメッシュは利用できません
		OM_AH_CREATE_CONTAINER_DERIVED_ERROR,    //拡張メッシュコンテナの領域確保に失敗
		OM_AH_COPYCONTAINERNAME_ERROR,           //コンテナ名のコピーに失敗
		OM_AH_CLONE_MESH_ERROR,                  //メッシュのクローンに失敗
		OM_AH_MTRAL_TEX_ADJA_ALLOC_ERROR,        //マテリアル、テクスチャ、隣接データの領域確保に失敗
		OM_AH_BONEOFFSETMATRICES_ALLOC_ERROR,    //ボーンオフセットマトリックスの領域確保に失敗
		OM_AH_GENERATE_SKINMESH_ERROR,           //スキンメッシュの作成に失敗
		OM_MODELMANAGE_ALLOCATE_ERROR,           //モデル管理クラスの領域確保に失敗
		OM_MODELBASE_ALLOCATE_ERROR,             //モデルベースクラスの領域の確保に失敗
		OM_AH_ALLOC_ERROR,                       //階層メッシュ読み込み用クラスの領域の確保に失敗
		OM_AFRAME_ALLOC_ERROR,                   //フレームクラスの領域の確保に失敗
		OM_MODEL_ANIMATIONSET_ALLOC_ERROR,       //アニメーションセットの領域の確保に失敗
		OM_INPUT_ALLOCATE_ERROR,                 //入力管理クラスの領域確保に失敗
		OM_INPUT_INITIALIZE_ERROR,               //入力管理クラスの初期化に失敗
		OM_INPUT_DI_CREATE_ERROR,                //DirectInputの作成に失敗
		OM_INPUT_INIT_KEYBOARD_ERROR,            //キーボードデバイスの初期化に失敗
		OM_INPUT_INIT_MOUSE_ERROR,               //マウスデバイスの初期化に失敗
		OM_INPUT_INIT_PAD_ERROR,                 //ゲームパッドデバイスの初期化に失敗
		OM_ENGINE_ALLOCATE_ERROR,                //エンジンクラスの領域確保に失敗
		OM_ENGINE_INITIALIZE_ERROR,              //エンジンクラスの初期化に失敗
		OM_NEW_ERROR,                            //newによる領域確保に失敗
		OM_CREATE_LINE_OBJECT_ERROR,             //線描画オブジェクトの作成に失敗
		OM_CREATE_LINE_ALLOCATE_ERROR,           //線描画オブジェクトの領域確保に失敗
		OM_FILE_OPEN_ERROR,                      //ファイルのオープンに失敗
		OM_BILLBOARD_LOAD_RESOURCE_ERROR,        //ビルボードシェーダーの読み込みに失敗
		OM_BILLBOARD_DECLARE_ERROR,              //ビルボードの頂点宣言に失敗
		OM_PRIMITIVE_LOAD_RESOURCE_ERROR,        //プリミティブシェーダーの読み込みに失敗
		OM_PRIMITIVE_DECLARE_ERROR,              //プリミティブの頂点宣言に失敗
		OM_PARTICLE_LOAD_RESOURCE_ERROR,         //パーティクルシェーダーの読み込みに失敗
		OM_PARTICLE_DECLARE_ERROR,               //パーティクルの頂点宣言に失敗
		OM_XFILE_NOSKIN_LOAD_RESOURCE_ERROR,     //Ｘファイルシェーダー（スキンなし）の読み込みに失敗
		OM_XFILE_LOAD_RESOURCE_ERROR,            //Ｘファイルシェーダーの読み込みに失敗
		OM_XFILE_DECLARE_ERROR,                  //Ｘファイルの頂点宣言に失敗
		OM_BB_LOAD_RESOURCE_ERROR,               //バウンディングボックスシェーダーの読み込みに失敗
		OM_BB_DECLARE_ERROR,                     //バウンディングボックスの頂点宣言に失敗
		OM_BS_LOAD_RESOURCE_ERROR,               //バウンディングスフィアシェーダーの読み込みに失敗
		OM_BS_DECLARE_ERROR,                     //バウンディングスフィアの頂点宣言に失敗
		OM_CLIENT_CREATE_DIALOG_ERROR,           //ダイアログの作成に失敗
		OM_CLIENT_INIT_SOCKET_ERROR,             //ソケットの初期化に失敗
		OM_CLIENT_CREATE_SOCKET_ERROR,           //ソケットの作成に失敗
		OM_CLIENT_CONNECT_SERVER,                //サーバーへの接続に失敗
		OM_CLIENT_RECEIVE_DATA_START_ERROR,      //データ受信スレッドの開始に失敗
		OM_CLIENT_REJECT_SERVER,                 //サーバーに接続拒否されました
		OM_SERVER_CREATE_DIALOG_ERROR,           //ダイアログの作成に失敗
		OM_SERVER_INIT_SOCKET_ERROR,             //ソケットの初期化に失敗
		OM_SERVER_CREATE_SOCKET_ERROR,           //ソケットの作成に失敗
		OM_SERVER_ACCEPT_CLIENT_ERROR,           //クライアント受付スレッドの開始に失敗
		OM_SERVER_RECEIVE_DATA_START_ERROR,      //データ受信スレッドの開始に失敗
		OM_CANT_CHANGE_SCENE_ERROR,              //サブシーンからは元のシーンか次のサブシーン以外への遷移は禁止されています
		OM_CREATE_WINDOW_HEIGHT_ERROR,           //スクリーンの高さを超えるウインドウは作成できません
	};

	//エラーメッセージの出力先
	enum OutputTarget {
		OUT_MESSAGEBOX,   //メッセージボックス
		OUT_OUTPUTWINDOW, //出力ウィンドウ
	};

	//=============================================================================
	// コンストラクタ
	//=============================================================================
	DxSystemException(const OriginalMessage message);

	//=============================================================================
	// オリジナルエラーメッセージの表示
	// 引　数：const OutputTarget　　エラーメッセージの出力先（デフォルトはメッセージボックス）
	//=============================================================================
	void ShowOriginalMessage(const OutputTarget outputTarget = OUT_MESSAGEBOX);

private:

	//オリジナルエラーメッセージ
	OriginalMessage m_message;

	//-----------------------------------------------------------------------------
	// オリジナルエラーメッセージの取得
	// 戻り値：エラーメッセージ文字列
	// 引　数：const OriginalMessage オリジナルエラーメッセージ
	//-----------------------------------------------------------------------------
	const wchar_t* SearchOriginalMessage(const OriginalMessage message);
};